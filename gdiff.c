#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "gdiff.h"
#include "cfile.h"
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
    if(offset_type==ENCODING_OFFSET_START)
	writeUBytes(fh, GDIFF_VER4, GDIFF_VER_LEN);
    else if(offset_type==ENCODING_OFFSET_VERS_POS)
	writeUBytes(fh, GDIFF_VER5, GDIFF_VER_LEN);
    else if(offset_type==ENCODING_OFFSET_DC_POS)
	writeUBytes(fh, GDIFF_VER6, GDIFF_VER_LEN);
    else {
	printf("wtf, gdiff doesn't know offset_type(%u). bug.\n",offset_type);
	exit(1);
    }
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
		delta_pos, fh_pos, out_buff[0], (off_is_sbytes ? s_off: u_off), buffer->lb_tail->len);
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
    //ahem.  better error handling/returning needed. in time, in time...
    return 0;
}

signed int gdiffReconstructFile(int src_fh, int out_fh,
	struct PatchDeltaBuffer *PDBuff, unsigned int offset_type,
	unsigned int gdiff_version)
{
    unsigned char *cptr;
    unsigned long int len, x;
    unsigned long ver_pos=0, dc_pos=0;
    unsigned long int u_off;
    signed long int s_off;
    int off_is_sbytes, ob, lb;
    unsigned char cpy_buff[PATCHER_COPY_BUFFER_SIZE];
    if(offset_type==ENCODING_OFFSET_VERS_POS || offset_type==ENCODING_OFFSET_DC_POS)
	off_is_sbytes=1;
    else if(offset_type==ENCODING_OFFSET_START)
	off_is_sbytes=0;
    else {
	printf("wtf, unknown offset_type for reconstruction(%u)\n",offset_type);
	exit(1);
    }
    cptr= PDBuff->buffer;
    while(*cptr != 0) {
	printf("fh_pos(%lu), command(%u), cptr(%u)\n", ver_pos, *cptr, cptr - PDBuff->buffer);
	if(*cptr > 0 && *cptr <= 248) {
	    //add command
	    printf("add command\n");
	    if(*cptr >=247 && *cptr <= 248){
		if (*cptr==247)
		    lb=2;
		else if (*cptr==248)
		    lb=4;
		if(PDBuff->filled_len - (cptr - PDBuff->buffer) < lb + 1) {
		    printf("refreshing buffer for add command\n");
		    refreshPDBuffer(PDBuff, PDBuff->filled_len - (cptr - PDBuff->buffer));
		    cptr = PDBuff->buffer;
		    printf("after refreshing, cptr(%u)\n", *cptr);
		}
		len= readUnsignedBytes(cptr + 1, lb);
		cptr+=lb;
	    } else
		len=*cptr;
	    cptr++;
	    printf("first byte of add command(%u) is (%u)\n", *(cptr -1), *cptr);
	    //printf("add  command delta_pos(%lu), fh_pos(%lu), len(%u)\n", PDBuff->delta_pos, fh_pos, ccom);
	    //clen = MIN(buff_filled - (cptr - commands), ccom);
	    //printf("len(%lu), clen(%lu)\n", *cptr, clen);
	    ver_pos += len;
	    while(len) {
		x=MIN(len, PDBuff->filled_len - (cptr - PDBuff->buffer));
		printf("   adding, len(%lu), x(%lu)\n", len, x);
		if((write(out_fh, cptr, x))!=x) {
		    printf("Weird... error writing to out_fh\n");
		    exit(1);
		}
		len-=x;
		if(len) {
		    printf("refreshing buffer w/in add\n");
		    refreshPDBuffer(PDBuff, 0);
		    cptr=PDBuff->buffer;
		} else 
		    cptr+=x;
	    }
	    printf("left with cptr(%u)\n", *cptr);
	} else if(*cptr >= 249 ) {
	    //copy command
	    printf("copy command cptr(%u), pos(%u)\n", *cptr, cptr - PDBuff->buffer );
	    if(*cptr >=249 && *cptr <= 251) {
		ob=2;
		if(*cptr==249)
		    lb=1;
		else if(*cptr==250)
		    lb=2;
		else if(*cptr==251)
		    lb=4;
	    } else if (*cptr >=252 && *cptr <=254) {
		ob=4;
		if(*cptr==252)
		    lb=1;
		if(*cptr==253)
		    lb=2;
		if(*cptr==254)
		    lb=4;
	    } else {
		ob=8;
		lb=4;
	    }
	    if(PDBuff->filled_len - (cptr - PDBuff->buffer) < 1 + ob + lb) {
		printf("refreshing buffer in copy, cptr(%u), filled_len(%u), (%u)\n",
		       PDBuff->filled_len, cptr - PDBuff->buffer,
		       PDBuff->filled_len - (cptr - PDBuff->buffer));
		refreshPDBuffer(PDBuff, PDBuff->filled_len - (cptr - PDBuff->buffer));
		cptr = PDBuff->buffer;
		printf("cptr(%u)\n", *cptr);
	    }
	    cptr++;
	    if(off_is_sbytes) {
		s_off=readSignedBytes(cptr, ob);
		//convertSBytesChar(out_buff + 1, s_off, ob);
	    } else {
		//convertUBytesChar(out_buff + 1, u_off, ob);
		u_off=readUnsignedBytes(cptr, ob);
	    }
	    cptr+=ob;
	    len = readUnsignedBytes(cptr, lb);
	    printf("copy len(%lu)\n", len);
	    cptr+=lb;
	    if(offset_type!=ENCODING_OFFSET_START) {
		if(offset_type==ENCODING_OFFSET_VERS_POS)
		    u_off = ver_pos + s_off;
		else //ENCODING_DC_POS
		    dc_pos = u_off = dc_pos + s_off;
	    }
	    /*printf("copy command delta_pos(%lu), fh_pos(%lu), type(%u), offset(%ld), ref_pos(%lu) len(%lu)\n",
		delta_pos, fh_pos, ccom, offset, fh_pos + offset, len);*/
	    if(lseek(src_fh, u_off, SEEK_SET)!= u_off) {
		printf("well that's weird, couldn't lseek.\n");
		exit(EXIT_FAILURE);
	    }
	    ver_pos+=len;
	    while(len) {
		x = MIN(PATCHER_COPY_BUFFER_SIZE, len);
		printf("copying (%u) bytes of len(%u)\n", x ,len);
		if(read(src_fh, cpy_buff, MIN(PATCHER_COPY_BUFFER_SIZE, len)) != x) {
		    printf("hmm, error reading src_fh.\n");
		    //printf("clen(%u), len(%lu)\n", clen, len);
		    exit(EXIT_FAILURE);
		}
		if(write(out_fh, cpy_buff, x) != x){
		    printf("hmm, error writing the versionned file.\n");
		    exit(EXIT_FAILURE);
		}
		len -= x;
	    }
	}
	if(cptr - PDBuff->buffer == PDBuff->filled_len) {
	    printf("refreshing buffer\n");
	    //printf("refreshing buffer: cptr(%u)==buff_filled(%u)\n", cptr - commands, buff_filled);
	    refreshPDBuffer(PDBuff, 0);
	    cptr = PDBuff->buffer;
	    //continue;
	}
	/*if(cptr == commands + buff_filled) {
	    printf("refreshing buffer: cptr(%u)==buff_filled(%u)\n", cptr - commands, buff_filled);
	    if((buff_filled=read(delta_fh, commands, 512))==0){
		printf("ahem.  the delta file is empty?\n");
		exit(EXIT_FAILURE);
	    }
	    cptr=commands;
	}*/
    }
    return 0;
}
