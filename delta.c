#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
//#include "adler32.h"
#include "delta.h"
#include "bit-functions.h"
/* this is largely based on the algorithms detailed in randal burn's various papers.
   Obviously credit for the alg's go to him, although I'm the one who gets the dubious
   credit for bugs in the implementation of said algorithms... */
#define LOOKBACK_SIZE 100000
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

void updateDCCopyStats(struct DCStats *stats, signed long pos_offset, signed long dc_offset, unsigned long len)
{
    stats->copy_count++;
    stats->copy_pos_offset_bytes[MAX( MIN(signedBytesNeeded(pos_offset) - 1, 0), 5)]++;
    stats->copy_rel_offset_bytes[MAX(MIN(signedBytesNeeded(dc_offset) -1, 0), 5)]++;
    stats->copy_len_bytes[MAX(MIN(unsignedBytesNeeded(len)-1, 0),5)]++;
}

void updateDCAddStats(struct DCStats *stats, unsigned long len)
{
    stats->add_count++;
    
}

void undoDCCopyStats(struct DCStats *stats, signed long pos_offset, unsigned long len)
{
    stats->copy_count++;
    stats->copy_pos_offset_bytes[MAX( MIN(signedBytesNeeded(pos_offset) - 1, 0), 5)]--;
//    stats->copy_rel_offset_bytes[MAX(MIN(signedBytesNeeded(dc_offset) -1, 0), 5)]--;
    stats->copy_len_bytes[MAX(MIN(unsignedBytesNeeded(len)-1, 0),5)]--;
}

void undoDCAddStats(struct DCStats *stats, unsigned long len)
{
    stats->add_count--;
    
}



void DCBufferTruncate(struct CommandBuffer *buffer, unsigned long len)
{
    //get the tail to an actual node.
    DCBufferDecr(buffer);
    //printf("truncation: \n");
    while(len) {
	/* should that be less then or equal? */
	if (buffer->lb_tail->len <= len) {
	    len -= buffer->lb_tail->len;
	    printf("    whole removal of type(%u), offset(%lu), len(%lu)\n",
		(*buffer->cb_tail & (1 << buffer->cb_tail_bit)) >> buffer->cb_tail_bit, buffer->lb_tail->offset,
		buffer->lb_tail->len);
	    DCBufferDecr(buffer);
	    buffer->count--;
	} else {
	    printf("    partial adjust of type(%u), offset(%lu), len(%lu) is now len(%lu)\n",
		(*buffer->cb_tail & (1 << buffer->cb_tail_bit))>buffer->cb_tail_bit, buffer->lb_tail->offset,
		buffer->lb_tail->len, buffer->lb_tail->len - len);
	    buffer->lb_tail->len -= len;
	    len=0;
	}
    }
    DCBufferIncr(buffer);
}



void DCBufferFlush(struct CommandBuffer *buffer, unsigned char *ver, int fh)
{
    unsigned char *ptr, clen;
    unsigned long fh_pos=0;
    //unsigned long offset;
    signed long s_off;
    unsigned long u_off;
    unsigned long delta_pos=0, dc_pos=0;
    unsigned long copies=0, adds_in_buff=0, adds_in_file=0;
    int lb, ob;
    unsigned char type, out_buff[256];
    printf("commands in buffer(%lu)\n", buffer->count);
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    *out_buff=1;
    write(fh, out_buff, 1);
    while(buffer->count--){
	if((*buffer->cb_tail & (1 << buffer->cb_tail_bit))>0) {
	    ptr=ver + buffer->lb_tail->offset;
	    type=DC_COPY;
	} else if ((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==0){
	    type=DC_ADD;
	} else {
	    printf("wtf...\n");
	}
	switch(type)
	{
	case DC_ADD:
	    //offset=0;
	    /*printf("add command, delta_pos(%lu), fh_pos(%lu), len(%lu), broken into '%lu' commands\n",
		delta_pos, fh_pos, buffer->lb_tail->len, buffer->lb_tail->len/248 + (buffer->lb_tail->len % 248 ? 1 : 0));
	    adds_in_buff++;
	    u_off=buffer->lb_tail->len;*/
	    //u_off = buffer->lb_tail->len / 248 > 0 ? 1 : 0;
	    /*if(u_off > 246) {
		if(u_off <= 0xffff) {
		    clen=247;
		} else {
		    clen=248;
		}
	    } else {
		clen=u_off;
	    }
	    write(fh, &clen, 1);
	    if(clen > 246) {
		if(clen==247){
		    
		} else if(clen==248) {
		    
		}
	    }*/
	    while(buffer->lb_tail->len){
		adds_in_file++;
		clen=MIN(buffer->lb_tail->len, 248);//modded
		if(u_off)
		    printf("    writing add command offset(%lu), len(%u)\n", buffer->lb_tail->offset, clen);
		write(fh, &clen, 1);
		write(fh, ver + buffer->lb_tail->offset, clen);
		//offset+=clen;
		fh_pos+=clen;
		delta_pos += clen + 1;
		buffer->lb_tail->len -=clen;
		buffer->lb_tail->offset += clen;
	    }
	    break;
	case DC_COPY:
	    copies++;
	    s_off = (signed long)buffer->lb_tail->offset - (signed long)dc_pos;
	    u_off = abs(s_off);
	    //printf("s_offset(%d) ob: ");
	    ob=signedBytesNeeded(s_off);
	    //printf("lb: ");
	    lb=unsignedBytesNeeded(buffer->lb_tail->len);
	    if(ob <= 2 && lb ==1)
		clen=249;
	    else if(ob <= 2 && lb <=2)
		clen=250;
	    else if(ob <= 2 && lb <=4)
		clen=251;
	    else if(ob <= 4 && lb <=1)
		clen=252;
	    else if(ob <= 4 && lb <=2)
		clen=253;
	    else if(ob <= 4 && lb <=4)
		clen=254;
	    else
		clen=255;
	    printf("writing copy command delta_pos(%lu), fh_pos(%lu), type(%u), offset(%ld), len(%lu)\n",
		delta_pos, fh_pos, clen, s_off, buffer->lb_tail->len);
	    write(fh, &clen, 1);
	    delta_pos++;
	    if(clen >= 249 && clen <= 251) {
		out_buff[0] = (u_off & 0xff00) >> 8;
		if(s_off < 0 ) {
		    //printf("num is signed, initial byte(%u), ", out_buff[0]);
		    out_buff[0] |= 0x80;
		    //printf("final byte(%u)\n", out_buff[0]);
		}
		out_buff[1] = (u_off & 0x00ff);
		write(fh, out_buff, 2);
		delta_pos+=2;
	    } else if(clen>=252 && clen <= 254){
		out_buff[0] = (u_off & 0xff000000) >> 8*3;
		if(s_off < 0)
		    out_buff[0] |= 0x80;
		out_buff[1] = (u_off & 0x00ff0000) >> 8*2;
		out_buff[2] = (u_off & 0x0000ff00) >> 8;
		out_buff[3] = (u_off & 0x000000ff);
		write(fh, out_buff, 4);
		delta_pos+=4;
	    } else {
		out_buff[0] = (u_off & 0xff00000000000000) >> 8*7;
		if(s_off < 0)
		    out_buff[0] |= 0x80;
		out_buff[1] = (u_off & 0x00ff000000000000) >> 8*6;
		out_buff[2] = (u_off & 0x0000ff0000000000) >> 8*5;
		out_buff[3] = (u_off & 0x000000ff00000000) >> 8*4;
		out_buff[4] = (u_off & 0x00000000ff000000) >> 8*3;
		out_buff[5] = (u_off & 0x0000000000ff0000) >> 8*2;
		out_buff[6] = (u_off & 0x000000000000ff00) >> 8;
		out_buff[7] = (u_off & 0x00000000000000ff);
		write(fh, out_buff, 8);
		delta_pos+=8;
	    }
	    if(clen==249 || clen == 252){
		out_buff[0] = (buffer->lb_tail->len & 0xff);
		write(fh, out_buff, 1);
		delta_pos++;
	    } else if(clen == 250 || clen==253){
		out_buff[0] = (buffer->lb_tail->len & 0xff00) >> 8;
		out_buff[1] = (buffer->lb_tail->len & 0x00ff);
		write(fh, out_buff, 2);
		delta_pos+=2;
	    } else {
		out_buff[0] = (buffer->lb_tail->len & 0xff000000) >> 24;
		out_buff[1] = (buffer->lb_tail->len & 0x00ff0000) >> 16;
		out_buff[2] = (buffer->lb_tail->len & 0x0000ff00) >> 8;
		out_buff[3] = (buffer->lb_tail->len & 0x000000ff);
		write(fh, out_buff, 4);
		delta_pos+=4;
	    }
	    fh_pos += buffer->lb_tail->len;
	    dc_pos += s_off;
	    break;
	}
	//DCBufferDecr(buffer);
	DCBufferIncr(buffer);
    }
    out_buff[0]=0;
    write(fh, out_buff, 1);
    printf("Buffer statistics- copies(%lu), adds(%lu)\n    copy ratio=(%f%%), add ratio(%f%%)\n",
	copies, adds_in_buff, ((float)copies)/((float)(copies + adds_in_buff))*100,
	((float)adds_in_buff)/((float)copies + (float)adds_in_buff)*100);
    printf("adds in file(%lu), average # of commands per add(%f)\n", adds_in_file,
	((float)adds_in_file)/((float)(adds_in_buff)));
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
    unsigned char *vc, *va, *vs, *vm, *rm; //va=adler start, vs=first non-encoded byte.
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
    printf("load factor=%f%%\n", ((float)empties)/(float)(ref_len-seed_len-good_collisions)*(float)100);
    printf("bad collisions=%f%%\n", ((float)bad_collisions) / (float)(ref_len-seed_len-good_collisions)*(float)100);
    printf("good collisions=%f%%\n", ((float)good_collisions)/(float)(ref_len-seed_len)*100);
    /*for(x=0; x < ref_len - seed_len; x++){
	if (hr[x])
	    printf("hr[%u]='%lu'\n", x, hr[x]);
    }*/
    printf("version run:\n");
    printf("creating lookback buffer\n");
    DCBufferInit(&buffer, 10000000);
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
		//printf("bad collision(%lu).\n", (unsigned char *)vc - (unsigned char*)ver);
		bad_collisions++;
		vc++;
		continue;
	    }
	    printf("good collision(%lu):", (unsigned long)((unsigned char *)vc - (unsigned char*)ver));
	    good_collisions++;
	    x=0;
	    vm = vc;
	    rm = ref + hr[index];
	    while(vm > ver && rm > ref &&
		*(vm -1)==*(rm -1)) {
		vm--;
		rm--;
	    }
	    len = (vc - vm) + seed_len;
	    //printf("prefix len(%lu), ",len-seed_len);
	    while(vm + len < ver + ver_len && rm + len < ref + ref_len && vm[len] == rm[len]) {
		len++;
	    }
	    //printf("couldn't match %u==%u\n", vm[len], rm[len]);
	    printf("vstart(%lu), rstart(%lu), len(%lu)\n", (unsigned long)((unsigned char*)vm - (unsigned char*)ver),
		(unsigned long)(rm -ref), len);
	    if (vs <= vm) {
		if (vs < vm) {
		    printf("    adding vstart(%lu), len(%lu), vend(%lu): (vs < vm)\n",
			(unsigned long)(vs -ver), (unsigned long)(vm-vs), (unsigned long)(vm - ver));
		    //DCBufferAddCmd(&buffer, DC_ADD, vs -ver, (vc-x) -vs);
		    DCBufferAddCmd(&buffer, DC_ADD, vs -ver, vm - vs);
		    adds++;
		}
		printf("    copying offset(%lu), len(%lu)\n", (unsigned long)(vm -ver), len);
		//DCBufferAddCmd(&buffer, DC_COPY, (vc-x) - ver, len);
		//DCBufferAddCmd(&buffer, DC_COPY, hr[index] -x, len +x);
		DCBufferAddCmd(&buffer, DC_COPY, rm - ref, len);
	    } else if (vm < vs) {
		printf("    truncating(%lu) bytes: (vm < vs)\n", (unsigned long)(vs - vm));
		DCBufferTruncate(&buffer, vs - vm);
		printf("    replacement copy: offset(%lu), len(%lu)\n", (unsigned long)(rm - ref), len);
		DCBufferAddCmd(&buffer, DC_COPY, rm -ref, len);
		truncations++;
	    } else {
		printf("what in the fuck... hit 3rd conditional on correction.  this means what?\n");
		exit(EXIT_FAILURE);
	    }
	    copies++;
	    vs = vm + len ;
	    vc = vs -1;
	} else {
	    //printf("no match(%lu)\n", vc -ver);
	}
	vc++;
    }
    if (vs -ver != ver_len)
	DCBufferAddCmd(&buffer, DC_ADD, vs -ver, ver_len - (vs -ver));
    printf("version summary:\n");
    printf("good collisions(%f%%)\n", (float)good_collisions/(float)(good_collisions+bad_collisions)*100);
    printf("bad  collisions(%f%%)\n",(float)bad_collisions/(float)(good_collisions+bad_collisions)*100);
    printf("commands in buffer, copies(%lu), adds(%lu), truncations(%lu)\n", copies, adds, truncations);
    printf("\n\nflushing command buffer...\n\n\n");
    DCBufferFlush(&buffer, ver, out_fh);
    return NULL;
}

//unsigned long hash_it(unsigned long

void DCBufferIncr(struct CommandBuffer *buffer)
{
    //printf("   incr: cb was offset(%lu)-bit(%u),", buffer->cb_tail - buffer->cb_start, buffer->cb_tail_bit);
    buffer->lb_tail = (buffer->lb_end==buffer->lb_tail) ? buffer->lb_start : buffer->lb_tail + 1;
    if (buffer->cb_tail_bit >= 7) {
	buffer->cb_tail_bit = 0;
	buffer->cb_tail = (buffer->cb_tail == buffer->cb_end) ? buffer->cb_start : buffer->cb_tail + 1;
    } else {
	buffer->cb_tail_bit++;
    }
    //printf(" now is offset(%lu)-bit(%u)\n", buffer->cb_tail - buffer->cb_start, buffer->cb_tail_bit);
}

void DCBufferDecr(struct CommandBuffer *buffer)
{
    //printf("   decr: cb was offset(%lu)-bit(%u),", buffer->cb_tail - buffer->cb_start, buffer->cb_tail_bit);
    buffer->lb_tail--;
    if (buffer->cb_tail_bit != 0) {
	buffer->cb_tail_bit--;
    } else {
	buffer->cb_tail = (buffer->cb_tail == buffer->cb_start) ? buffer->cb_end : buffer->cb_tail - 1;
	buffer->cb_tail_bit=7;
    }
    //printf(" now is offset(%lu)-bit(%u)\n", buffer->cb_tail - buffer->cb_start, buffer->cb_tail_bit);
}

void DCBufferAddCmd(struct CommandBuffer *buffer, int type, unsigned long offset, unsigned long len)
{
    if(buffer->count == buffer->max_commands) {
	printf("shite, buffer full.\n");
	exit(EXIT_FAILURE);
    }
    buffer->lb_tail->offset = offset;
    buffer->lb_tail->len = len;
    if (type==DC_ADD)
	*buffer->cb_tail &= ~(1 << buffer->cb_tail_bit);
    else
	*buffer->cb_tail |= (1 << buffer->cb_tail_bit);
    //printf("   addcmd desired value(%u), actual (%u)\n", type,
	//(*buffer->cb_tail & (1 << buffer->cb_tail_bit)) >> buffer->cb_tail_bit);
    buffer->count++;
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
