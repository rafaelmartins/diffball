#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "delta.h"
#include "gdiff.h"
#include "bit-functions.h"

signed int gdiffEncodeDCBuffer(struct CommandBuffer *buffer, 
    unsigned int offset_type, unsigned char *ver, int fh)
{
    unsigned char *ptr, clen;
    unsigned long fh_pos=0;
    //unsigned long offset;
    signed long s_off;
    unsigned long u_off;
    unsigned long delta_pos=0, dc_pos=0;
    unsigned long copies=0, adds_in_buff=0, adds_in_file=0;
    int lb, ob, off_is_sbytes;
    unsigned char type, out_buff[256];
    if(offset_type==ENCODING_OFFSET_VERS_POS || offset_type==ENCODING_OFFSET_DC_POS)
	off_is_sbytes=1;
    else
	off_is_sbytes=0;
    printf("commands in buffer(%lu)\n", buffer->count);
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    writeUBytes(fh, GDIFF_MAGIC, GDIFF_MAGIC_LEN);
    writeUBytes(fh, GDIFF_VER4, GDIFF_VER_LEN);
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
	    adds_in_file++;
	    if(buffer->lb_tail->len <= 246) {
		writeUBytes(fh, buffer->lb_tail->len, 1);
		delta_pos++;
	    } else if (buffer->lb_tail->len <= 0xffff) {
		writeUBytes(fh, 247, 1);
		writeUBytes(fh, buffer->lb_tail->len, 2);
		delta_pos+=2;
	    } else if (buffer->lb_tail->len <= 0xffffffff) {
		writeUBytes(fh, 248, 1);
		writeUBytes(fh, buffer->lb_tail->len, 4);
		delta_pos+=4;
	    } else {
		printf("wtf, encountered an offset larger then int size.  croaking.\n");
		exit(1);
	    }
	    write(fh, ver + buffer->lb_tail->offset, buffer->lb_tail->len);
	    delta_pos += buffer->lb_tail->len;
	    fh_pos += buffer->lb_tail->len;
	    break;
	case DC_COPY:
	    copies++;//clean this up.
	    if(off_is_sbytes) {
		if(offset_type==ENCODING_OFFSET_VERS_POS)
		    s_off = (signed long)buffer->lb_tail->offset - (signed long)fh_pos;
		else if(offset_type==ENCODING_OFFSET_DC_POS)
		    s_off = (signed long)buffer->lb_tail->offset - (signed long)dc_pos;		
		u_off = abs(s_off);
		ob=signedBytesNeeded(s_off);
	    } else {
		u_off = buffer->lb_tail->offset;
		ob=unsignedBytesNeeded(u_off);
	    }
	    lb=unsignedBytesNeeded(buffer->lb_tail->len);
	    if(lb> INT_BYTE_COUNT) {
		printf("wtf, too large of len in gdiff encoding. dieing.\n");
		exit(1);
	    }
	    if(ob > LONG_BYTE_COUNT) {
		printf("wtf, too large of offset in gdiff encoding. dieing.\n");
		exit(1);
	    }
	    clen=1;
	    if(lb <= BYTE_BYTE_COUNT)
		lb=BYTE_BYTE_COUNT;
	    else if(lb <= SHORT_BYTE_COUNT)
		lb=SHORT_BYTE_COUNT;
	    else
		lb=INT_BYTE_COUNT;
	    if(ob<=SHORT_BYTE_COUNT) {
		ob=SHORT_BYTE_COUNT;
		if(lb == BYTE_BYTE_COUNT)
		    out_buff[0]=249;
		else if(lb == SHORT_BYTE_COUNT)
		    out_buff[0]=250;
		else
		    out_buff[0]=251;
	    } else if (ob<=INT_BYTE_COUNT) {
		ob=INT_BYTE_COUNT;
		if(lb == BYTE_BYTE_COUNT)
		    out_buff[0]=252;
		else if(lb == SHORT_BYTE_COUNT)
		    out_buff[0]=253;
		else
		    out_buff[0]=254;
	    } else {
		ob=LONG_BYTE_COUNT;
		out_buff[0]=255;
	    }
	    if(off_is_sbytes)
		convertSBytesChar(out_buff + 1, s_off, ob);
	    else 
		convertUBytesChar(out_buff + 1, u_off, ob);
	    clen = 1 + ob;
	    convertUBytesChar(out_buff + clen, buffer->lb_tail->len, lb);
	    clen+=lb;		
	    printf("writing copy command delta_pos(%lu), fh_pos(%lu), type(%u), offset(%ld), len(%lu)\n",
		delta_pos, fh_pos, clen, s_off, buffer->lb_tail->len);
	    if(clen!=write(fh, out_buff, clen)) {
		printf("shite, couldn't write copy command. eh?\n");
		exit(1);
	    }
	    fh_pos+=buffer->lb_tail->len;
	    delta_pos+=clen;
	    dc_pos += s_off;
	}
	DCBufferIncr(buffer);
    }
    writeUBytes(fh, 0,1);
    printf("Buffer statistics- copies(%lu), adds(%lu)\n    copy ratio=(%f%%), add ratio(%f%%)\n",
	copies, adds_in_buff, ((float)copies)/((float)(copies + adds_in_buff))*100,
	((float)adds_in_buff)/((float)copies + (float)adds_in_buff)*100);
    printf("adds in file(%lu), average # of commands per add(%f)\n", adds_in_file,
	((float)adds_in_file)/((float)(adds_in_buff)));
    return 0;

}


