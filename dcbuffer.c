#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "dcbuffer.h"
#include "cfile.h"
#include "bit-functions.h"
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

/*void DCBufferFlush(struct CommandBuffer *buffer, unsigned char *ver, int fh)
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
    gz(fh, out_buff, 1);
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
	    printf("add command, delta_pos(%lu), fh_pos(%lu), len(%lu), broken into '%lu' commands\n",
		delta_pos, fh_pos, buffer->lb_tail->len, buffer->lb_tail->len/248 + (buffer->lb_tail->len % 248 ? 1 : 0));
	    adds_in_buff++;
	    u_off=buffer->lb_tail->len;
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
		if(convertSBytesChar(out_buff, s_off, 2)) {
		    printf("shite, too large of signed value!\n");
		    exit(1);
		}
		write(fh, out_buff, 2);
		delta_pos+=2;
	    } else if(clen>=252 && clen <= 254){
		if(convertSBytesChar(out_buff, s_off, 4)) {
		    printf("shite, too large of signed value!\n");
		    exit(1);
		}
		write(fh, out_buff, 4);
		delta_pos+=4;
	    } else {
		if(convertSBytesChar(out_buff, s_off, 8)) {
		    printf("shite, too large of signed value!\n");
		    exit(1);
		}
		write(fh, out_buff, 8);
		delta_pos+=8;
	    }
	    if(clen==249 || clen == 252){
		if(convertUBytesChar(out_buff, buffer->lb_tail->len, 1)) {
		    printf("shite, too large of signed value!\n");
		    exit(1);
		}
		write(fh, out_buff, 1);
		delta_pos++;
	    } else if(clen == 250 || clen==253){
		if(convertUBytesChar(out_buff, buffer->lb_tail->len, 2)) {
		    printf("shite, too large of signed value!\n");
		    exit(1);
		}
		write(fh, out_buff, 2);
		delta_pos+=2;
	    } else {
		if(convertUBytesChar(out_buff, buffer->lb_tail->len, 4)) {
		    printf("shite, too large of signed value!\n");
		    exit(1);
		}
		write(fh, out_buff, 4);
		delta_pos+=4;
	    }
	    fh_pos += buffer->lb_tail->len;
	    dc_pos += s_off;
	    break;
	}
	DCBufferIncr(buffer);
    }
    out_buff[0]=0;
    write(fh, out_buff, 1);
    printf("Buffer statistics- copies(%lu), adds(%lu)\n    copy ratio=(%f%%), add ratio(%f%%)\n",
	copies, adds_in_buff, ((float)copies)/((float)(copies + adds_in_buff))*100,
	((float)adds_in_buff)/((float)copies + (float)adds_in_buff)*100);
    printf("adds in file(%lu), average # of commands per add(%f)\n", adds_in_file,
	((float)adds_in_file)/((float)(adds_in_buff)));
}*/
