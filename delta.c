#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "adler32.h"
#include "delta.h"
/* this is largely based on the algorithms detailed in randal burn's various papers.
   Obviously credit for the alg's go to him, although I'm the one who gets the dubious
   credit for bugs in the implementation of said algorithms... */
#define LOOKBACK_SIZE 100000
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

void DCBufferIncr(struct CommandBuffer *buffer)
{
    buffer->lb_tail = (buffer->lb_end==buffer->lb_tail) ? buffer->lb_start : buffer->lb_tail + 1;
    if (buffer->cb_tail_bit == 7) {
	buffer->cb_tail_bit = 0;
	buffer->cb_tail = (buffer->cb_tail == buffer->cb_end) ? buffer->cb_start : buffer->cb_tail + 1;
    } else {
	buffer->cb_tail_bit++;
    }
}

void DCBufferDecr(struct CommandBuffer *buffer)
{
    buffer->lb_tail--;
    if (buffer->cb_tail_bit) {
	buffer->cb_tail_bit--;
    } else {
	buffer->cb_tail = (buffer->cb_tail == buffer->cb_start) ? buffer->cb_end : buffer->cb_tail - 1;
	buffer->cb_tail_bit==0;
    }    
}

void DCBufferAddCmd(struct CommandBuffer *buffer, int type, unsigned long offset, unsigned long len)
{
    buffer->lb_tail->offset = offset;
    buffer->lb_tail->len = len;
    if (type==DC_ADD)
	*buffer->cb_tail &= ~(1 << buffer->cb_tail_bit);
    else
	*buffer->cb_tail |= (1 << buffer->cb_tail_bit);
    buffer->count++;
    DCBufferIncr(buffer);
}

void DCBufferTruncate(struct CommandBuffer *buffer, unsigned long len)
{
    //get the tail to an actual node.
    DCBufferDecr(buffer);
    while(len) {
	/* should that be less then or equal? */
	if (buffer->lb_tail->len <= len) {
	    len -= buffer->lb_tail->len;
	    DCBufferDecr(buffer);
	    buffer->count--;
	} else {
	    buffer->lb_tail->len -= len;
	    len=0;
	}
    }
    DCBufferIncr(buffer);
}

void DCBufferInit(struct CommandBuffer *buffer, unsigned long max_commands)
{
    buffer->count=0;
    buffer->max_commands = max_commands + (max_commands % 8 ? 1 : 0);
    printf("asked for size(%lu), using size(%lu)\n", max_commands, buffer->max_commands);
    if((buffer->cb_start = (char *)malloc(buffer->max_commands/8))==NULL){
	perror("shite, malloc failed\n");
	exit(EXIT_FAILURE);
    }
    buffer->cb_head = buffer->cb_tail = buffer->cb_start;
    buffer->cb_end = buffer->cb_start + (buffer->max_commands/8) -1;
    buffer->cb_head_bit = buffer->cb_tail_bit = 0;
    if((buffer->lb_start = (struct DCLoc *)malloc(sizeof(struct DCLoc) * buffer->max_commands))==NULL){
	perror("shite, malloc failed\n");
	exit(EXIT_FAILURE);
    }
    buffer->lb_head = buffer->lb_tail = buffer->lb_start;
    buffer->lb_end = buffer->lb_start + buffer->max_commands -1;
}

int bytesNeeded(unsigned long y)
{
    unsigned int x=0;
    while((y = y >>1) > 0)
	x++;
    return x;
}

void DCBufferFlush(struct CommandBuffer *buffer, unsigned char *ver, int fh)
{
    unsigned char *ptr, len;
    unsigned long fh_pos=0;
    unsigned long offset;
    unsigned long copies=0, adds=0;
    int lb, ob;
    unsigned char type, out_buff[256];
    printf("commands in buffer(%lu)\n", buffer->count);
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_start;
    buffer->cb_tail_bit = 0;
    //DCBufferDecr(buffer);
    while(buffer->count--){
	/* this lil method *CANNOT BE WORKING RIGHT* */
	if((*buffer->cb_tail & (1 << buffer->cb_tail_bit))>0) {
	    ptr=ver + buffer->lb_tail->offset;
	    type=DC_COPY;
	} else if ((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==0){
	    //ptr=ver + buffer->lb_tail->offset;
	    type=DC_ADD;
	} else {
	    printf("wtf...\n");
	}
	switch(type)
	{
	case DC_ADD:
	    offset=0;
	    printf("add command, offset(%lu), len(%lu)\n", buffer->lb_tail->offset, buffer->lb_tail->len);
	    while(buffer->lb_tail->len){
		adds++;
		len=MIN(buffer->lb_tail->len, 248);
		printf("    writing add command offset(%lu), len(%lu)\n", buffer->lb_tail->offset + offset, len);
		write(fh, &len, 1);
		write(fh, ptr + offset, len);
		//fprintf(fh, '%c%*c', (unsigned char)len, len, ptr + offset);
		offset+=len;
		fh_pos+=len;
		buffer->lb_tail->len -=len;
	    }
	    break;
	case DC_COPY:
	    copies++;
	    printf("writing copy command, offset(%lu), len(%lu)\n", buffer->lb_tail->offset, buffer->lb_tail->len);
	    lb=bytesNeeded(buffer->lb_tail->len);
	    ob=bytesNeeded(buffer->lb_tail->offset);
	    if(ob <= 2 && lb ==1)
		len=249;
	    else if(ob <= 2 && lb <=2)
		len=250;
	    else if(ob <= 2 && lb <=4)
		len=251;
	    else if(ob <= 4 && lb <=1)
		len=252;
	    else if(ob <= 4 && lb <=2)
		len=253;
	    else if(ob <= 4 && lb <=4)
		len=254;
	    else
		len=255;
	    write(fh, &len, 1);
	    fh_pos++;
	    if(len >= 249 && len <= 251) {
		out_buff[0] = (buffer->lb_tail->offset & 0xff00) >> 2;
		out_buff[1] = (buffer->lb_tail->offset & 0x00ff);
		write(fh, out_buff, 2);
		fh_pos+=2;
	    } else if(len>=252 && len <= 254){
		out_buff[0] = (buffer->lb_tail->offset & 0xff000000) >> 8*3;
		out_buff[1] = (buffer->lb_tail->offset & 0x00ff0000) >> 8*2;
		out_buff[2] = (buffer->lb_tail->offset & 0x0000ff00) >> 8;
		out_buff[3] = (buffer->lb_tail->offset & 0x000000ff);
		write(fh, out_buff, 4);
		fh_pos+=4;
	    } else {
		out_buff[0] = (buffer->lb_tail->offset & 0xff00000000000000) >> 8*7;
		out_buff[1] = (buffer->lb_tail->offset & 0x00ff000000000000) >> 8*6;
		out_buff[2] = (buffer->lb_tail->offset & 0x0000ff0000000000) >> 8*5;
		out_buff[3] = (buffer->lb_tail->offset & 0x000000ff00000000) >> 8*4;
		out_buff[4] = (buffer->lb_tail->offset & 0x00000000ff000000) >> 8*3;
		out_buff[5] = (buffer->lb_tail->offset & 0x0000000000ff0000) >> 8*2;
		out_buff[6] = (buffer->lb_tail->offset & 0x000000000000ff00) >> 8;
		out_buff[7] = (buffer->lb_tail->offset & 0x00000000000000ff);
		write(fh, out_buff, 8);
		fh_pos+=8;
	    }
	    if(len==249 || len == 252){
		out_buff[0] = (buffer->lb_tail->len & 0xff);
		write(fh, out_buff, 1);
		fh_pos++;
	    } else if(len == 250 || len==253){
		out_buff[0] = (buffer->lb_tail->len & 0xff00) >> 8;
		out_buff[1] = (buffer->lb_tail->len & 0x00ff);
		write(fh, out_buff, 2);
		fh_pos+=2;
	    } else {
		out_buff[0] = (buffer->lb_tail->len & 0xff000000) >> 24;
		out_buff[1] = (buffer->lb_tail->len & 0x00ff0000) >> 16;
		out_buff[2] = (buffer->lb_tail->len & 0x0000ff00) >> 8;
		out_buff[3] = (buffer->lb_tail->len & 0x000000ff);
		write(fh, out_buff, 4);
		fh_pos+=4;
	    }
	    /* note this doesn't handle anything larger in length the int.  needs a lot of work. */
	    break;
	}
	//DCBufferDecr(buffer);
	DCBufferIncr(buffer);
    }
    out_buff[0]=0;
    write(fh, out_buff, 1);
    printf("wrote out copies(%lu), adds(%lu)\n    copy ratio=(%f), add ratio(%f)\n",
	copies, adds, ((float)copies)/((float)(copies + adds))*100, ((float)adds)/((float)(copies + adds))*100);
}


inline unsigned long hash_it(unsigned long chk, unsigned long tbl_size)
{
    return chk % tbl_size;
}

char *OneHalfPassCorrecting(unsigned char *ref, unsigned long ref_len,
    unsigned char *ver, unsigned long ver_len, unsigned int seed_len, int out_fh)
{
    unsigned long *hr; //reference hash table.
    unsigned long x, index, len;
    unsigned long s1, s2;
    unsigned long empties=0, good_collisions=0, bad_collisions=0;
    unsigned char *vc, *va, *vs, *vm; //va=adler start, vs=first non-encoded byte.
    struct CommandBuffer buffer;
    unsigned long copies=0, adds=0, truncations=0;
    s1=s2=0;
    if((hr=(unsigned long*)malloc(sizeof(unsigned long)*(ref_len - seed_len)))==NULL) {
	perror("Shite.  couldn't allocate needed memory for reference hash table.\n");
	exit(EXIT_FAILURE);
    }
    // init the bugger==0
    for(x=0; x < ref_len - seed_len; x++)
	hr[x] = 0;
    empties++;
    for(x=0; x < seed_len; x++) {
        s1 += ref[x]; s2 += s1;
    }
    hr[hash_it((s2 <<16) | (s1 & 0xffff), ref_len-seed_len)] =0;

    for(x=seed_len; x < ref_len - seed_len-1; x++) {
	s1 = s1 - ref[x-seed_len] + ref[x];
	s2 = s2 - (seed_len * ref[x-seed_len]) + s1;
	//hr[x - seed_len+1];
	index=hash_it((s2<<16)|(s1 & 0xffff), ref_len-seed_len);
	/*note this has the ability to overwrite offset 0...
	  but thats alright, cause a correcting alg if a match at offset1, will grab the offset 0 */
	if(hr[index]==0) {
	    empties++;
	    hr[index] = x - seed_len+1;
	} else {
	    if(memcmp((unsigned char *)ref+hr[index], (unsigned char*)ref+x, seed_len)==0){
		good_collisions++;
	    } else {
		bad_collisions++;
	    }
	}
    }
    printf("reference run:\n");
    printf("chksum array(%lu) genned\n", ref_len-seed_len);
    printf("load factor=%f\%\n", ((float)empties)/(float)(ref_len-seed_len-good_collisions)*(float)100);
    printf("bad collisions=%f\%\n", ((float)bad_collisions) / (float)(ref_len-seed_len-good_collisions)*(float)100);
    printf("good collisions=%f\%\n", ((float)good_collisions)/(float)(ref_len-seed_len)*100);
    /*for(x=0; x < ref_len - seed_len; x++){
	if (hr[x])
	    printf("hr[%u]='%lu'\n", x, hr[x]);
    }*/
    printf("version run:\n");
    printf("creating lookback buffer\n");
    DCBufferInit(&buffer, 100000);
    good_collisions = bad_collisions =0;
    vs = vc = (unsigned char*)ver;
    va=NULL; //this is the starting pt of the adler chksum of len(seed_len).
    while(vc + seed_len < (unsigned char *)ver + ver_len) {
	if(vc - seed_len < va) {
	    for(; va < vc; va++) {
		s1 = s1 - *va + va[seed_len];
		s2 = s2 - (seed_len * (unsigned char)*va) + s1;
	    }
	} else {
	    s1=s2=0;
	    for(va=vc; va < vc + seed_len; va++){
		s1 += *va;
		s2 += s1;
	    }
	    va=vc;
	}
	index = hash_it((s2 << 16) | (s1 & 0xffff), ref_len -seed_len);
	if(hr[index]) {
	    if (memcmp(ref+hr[index], vc, seed_len)!=0){
		printf("bad collision(%lu).\n", (unsigned char *)vc - (unsigned char*)ver);
		bad_collisions++;
		vc++;
		continue;
	    }
	    printf("good collision(%lu):", (unsigned char *)vc - (unsigned char*)ver);
	    good_collisions++;
	    x=0;
	    while(vc -x > ver && hr[index] -x > 0) {
		if(vc[-x]==ref[hr[index]-x]) {
		    x++;
		} else {
		    break;
		}
	    }
	    len = seed_len;
	    while(vc + len < ver + ver_len && hr[index] + x < ref_len) {
		if(vc[len]==ref[hr[index]+len]) {
		    len++;
		} else {
		    break;
		}
	    }
	    printf("vstart(%lu), rstart(%lu), len(%lu)\n", (unsigned char*)vc -x - (unsigned char*)ver,
		hr[index] - x, len);
	    if (vs <= vc - x) {
		if (vs < vc - x) {
		    printf("vs < vm, ego adding(%lu)\n",x);
		    //(vc -x) -ver
		    DCBufferAddCmd(&buffer, DC_ADD, vs -ver, (vc-x) -vs);
		    adds++;
		    //DCBufferAddCmd(&buffer, DC_COPY, vc - ver, len);
		}
		printf("    copying(%lu)\n", len);
		DCBufferAddCmd(&buffer, DC_COPY, (vc-x) - ver, len);
		copies++;
		vs = vc + len;
	    } else if (vc -x < vs) {
		// vs - (vc -x)
		printf("truncating(%lu), copying(%lu)\n", vs - (vc -x), x + len);
		DCBufferTruncate(&buffer, vs - (vc - x));
		DCBufferAddCmd(&buffer, DC_COPY, (vc -x) - ver, x + len);
		truncations++;
		copies++;
		vs = vc + len;
	    } else {
		printf("what in the fuck... hit 3rd conditional on correction.  this means what?\n");
		exit(EXIT_FAILURE);
	    }
	    vc +=len;
	} else {
	    printf("no match(%lu)\n", vc -ver);
	}
	vc++;
    }
    if (vs -ver != ver_len)
	DCBufferAddCmd(&buffer, DC_ADD, vs -ver, ver_len - (vs -ver));
    printf("version summary:\n");
    printf("good collisions(%f\%)\n", (float)good_collisions/(float)(good_collisions+bad_collisions)*100);
    printf("bad  collisions(%f\%)\n", (float)bad_collisions/(float)(good_collisions+bad_collisions)*100);
    printf("commands in buffer, copies(%lu), adds(%lu), truncations(%lu)\n", copies, adds, truncations);
    printf("flushing command buffer...\n");
    DCBufferFlush(&buffer, ver, out_fh);
    return NULL;
}

//unsigned long hash_it(unsigned long

