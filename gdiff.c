/*
  Copyright (C) 2003 Brian Harring

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, US 
*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "gdiff.h"
//#include "cfile.h"
#include "bit-functions.h"

signed int gdiffEncodeDCBuffer(struct CommandBuffer *buffer, 
    unsigned int offset_type, /*unsigned char *ver*/
    struct cfile *ver_cfh, /*int fh*/  struct cfile *out_fh)
{
    unsigned char /* *ptr,*/ clen;
    unsigned long fh_pos=0;
    signed long s_off;
    unsigned long u_off;
    unsigned long delta_pos=0, dc_pos=0;
    unsigned long copies=0, adds_in_buff=0, adds_in_file=0;
    int lb, ob, off_is_sbytes;
    unsigned char type, out_buff[256];
    unsigned int  add_buff_size=512;
    unsigned char add_buff[add_buff_size];
    unsigned int  bytes_read=0, temp=0;
    unsigned long  bytes_wrote=0;
    unsigned long count;
    unsigned long total_add_len=0, total_copy_len=0;
    if(offset_type==ENCODING_OFFSET_VERS_POS || offset_type==ENCODING_OFFSET_DC_POS)
		off_is_sbytes=1;
    else
		off_is_sbytes=0;
    printf("commands in buffer(%lu)\n", buffer->count);
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    convertUBytesChar(out_buff, GDIFF_MAGIC, GDIFF_MAGIC_LEN);
    cwrite(out_fh, out_buff, GDIFF_MAGIC_LEN);
    if(offset_type==ENCODING_OFFSET_START)
    	convertUBytesChar(out_buff, GDIFF_VER4, GDIFF_VER_LEN);
    else if(offset_type==ENCODING_OFFSET_VERS_POS)
		convertUBytesChar(out_buff, GDIFF_VER5, GDIFF_VER_LEN);
    else if(offset_type==ENCODING_OFFSET_DC_POS)
		convertUBytesChar(out_buff, GDIFF_VER6, GDIFF_VER_LEN);
    else {
		printf("wtf, gdiff doesn't know offset_type(%u). bug.\n",offset_type);
		exit(1);
    }
    cwrite(out_fh, out_buff, GDIFF_VER_LEN);
	count=buffer->count;
    while(buffer->count--){
		if((*buffer->cb_tail & (1 << buffer->cb_tail_bit))>0) {
	    	//ptr=ver + buffer->lb_tail->offset;
	    	type=DC_COPY;
		} else if ((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==0){
		    type=DC_ADD;
		} else {
		    printf("wtf...\n");
		}
		switch(type)
		{
		case DC_ADD:
		    printf("add command, delta_pos(%lu), fh_pos(%lu), len(%lu), ",
				delta_pos, fh_pos, buffer->lb_tail->len);
		    adds_in_buff++;
		    total_add_len += buffer->lb_tail->len;
		    u_off=buffer->lb_tail->len;
		    adds_in_file++;
		    if(buffer->lb_tail->len <= 246) {
				convertUBytesChar(out_buff, buffer->lb_tail->len, 1);
				cwrite(out_fh, out_buff, 1);
				delta_pos+=1;
				printf("type(%lu)\n", buffer->lb_tail->len);
		    } else if (buffer->lb_tail->len <= 0xffff) {
				convertUBytesChar(out_buff, 247, 1);
				convertUBytesChar(out_buff + 1, buffer->lb_tail->len, 2);
				cwrite(out_fh, out_buff, 3);
				delta_pos+=3;
				printf("type(%u)\n", 247);
				//printf("verifying, %u %u %u\n", out_buff[0], out_buff[1], out_buff[2]);
		    } else if (buffer->lb_tail->len <= 0xffffffff) {
				convertUBytesChar(out_buff, 248, 1);
				convertUBytesChar(out_buff + 1, buffer->lb_tail->len, 4);
				cwrite(out_fh, out_buff, 5);
				delta_pos+=5;
				printf("type(%u)\n", 248);
				//printf("verifying, %5.5s\n", out_buff);
		    } else {
				printf("wtf, encountered an offset larger then int size.  croaking.\n");
				exit(1);
		    }
		    //printf("cseeking to (%lu), got (%lu)\n",
		    //	buffer->lb_tail->offset, 
		    /*cseek(ver_cfh, buffer->lb_tail->offset, CSEEK_ABS);
		    bytes_wrote=0;
		    while(buffer->lb_tail->len - bytes_wrote > 0) {
		    	//printf("len(%lu), bytes_wrote(%lu), left(%lu)\n", 
		    	//buffer->lb_tail->len, bytes_wrote, buffer->lb_tail->len - bytes_wrote);
		    	temp = MIN(buffer->lb_tail->len - bytes_wrote, add_buff_size);
			    if((bytes_read=cread(ver_cfh, add_buff, temp))!=temp) {
			    	printf("add failure, offset(%lu), len(%lu)\n", 
			    	buffer->lb_tail->offset, buffer->lb_tail->len);
			    	printf("requested (%u) bytes, got (%u) bytes\n", temp, bytes_read);
			    	printf("shite, problem reading from versionned file.\n");
			    	exit(1);
			    }
			    cwrite(out_fh, add_buff, bytes_read);
			    bytes_wrote += bytes_read;
			}
		    delta_pos += buffer->lb_tail->len;
		    */
		    fh_pos += buffer->lb_tail->len;
		    break;
		case DC_COPY:
		    copies++;//clean this up.
		    total_copy_len += buffer->lb_tail->len;
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
				convertSBytesChar(out_buff + clen, s_off, ob);
		    else 
				convertUBytesChar(out_buff + clen, u_off, ob);
		    clen+= ob;
		    convertUBytesChar(out_buff + clen, buffer->lb_tail->len, lb);
		    clen+=lb;
		    printf("writing copy delta_pos(%lu), fh_pos(%lu), type(%u), offset(%ld), len(%lu)\n",
			delta_pos, fh_pos, out_buff[0], (off_is_sbytes ? s_off: u_off), buffer->lb_tail->len);
		    if(cwrite(out_fh, out_buff, clen)!=clen) {
				printf("shite, couldn't write copy command. eh?\n");
				exit(1);
		    }
		    fh_pos+=buffer->lb_tail->len;
		    delta_pos+=1 + ob + lb;
		    dc_pos += s_off;
		}
		DCBufferIncr(buffer);
    }
    convertUBytesChar(out_buff, 0, 1);
    cwrite(out_fh, out_buff, 1);
    dc_pos++;
    printf("wrote commands, now writing add data\n");
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    while(count--) {
    	if ((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==0){
    		cseek(ver_cfh, buffer->lb_tail->offset, CSEEK_ABS);
		    bytes_wrote=0;
		    while(buffer->lb_tail->len - bytes_wrote > 0) {
		    	//printf("len(%lu), bytes_wrote(%lu), left(%lu)\n", 
		    	//buffer->lb_tail->len, bytes_wrote, buffer->lb_tail->len - bytes_wrote);
		    	temp = MIN(buffer->lb_tail->len - bytes_wrote, add_buff_size);
			    if((bytes_read=cread(ver_cfh, add_buff, temp))!=temp) {
			    	printf("add failure, offset(%lu), len(%lu)\n", 
			    	buffer->lb_tail->offset, buffer->lb_tail->len);
			    	printf("requested (%u) bytes, got (%u) bytes\n", temp, bytes_read);
			    	printf("shite, problem reading from versionned file.\n");
			    	exit(1);
			    }
			    cwrite(out_fh, add_buff, bytes_read);
			    bytes_wrote += bytes_read;
			}
		    delta_pos += buffer->lb_tail->len;
    	}
    	DCBufferIncr(buffer);
    }
    convertUBytesChar(out_buff, 0, 1);
    cwrite(out_fh, out_buff, 1);
    dc_pos++;
    printf("Buffer statistics: copies(%lu), adds(%lu)\n    copy ratio=(%f%%), add ratio(%f%%)\n",
	copies, adds_in_buff, ((float)copies)/((float)(copies + adds_in_buff))*100,
	((float)adds_in_buff)/((float)copies + (float)adds_in_buff)*100);
    printf("Buffer statistics: average copy_len(%lu), average add_len(%lu)\n",
    (float)((float)total_copy_len/(float)copies), 
    (float)((float)total_add_len/(float)adds_in_buff));
    //printf("adds in file(%lu), average # of commands per add(%f)\n", adds_in_file,
	//((float)adds_in_file)/((float)(adds_in_buff)));
    //ahem.  better error handling/returning needed. in time, in time...
    return 0;
}

signed int gdiffReconstructDCBuff(struct cfile *patchf, struct CommandBuffer *dcbuff, 
	unsigned int offset_type, unsigned int gdiff_version)
{

    //unsigned char *cptr;
	const unsigned int buff_size = 4096;
    unsigned char buff[buff_size];
	unsigned long int len;
    unsigned long ver_pos=0, dc_pos=0;
    unsigned long int u_off;
    signed long int s_off;
    int off_is_sbytes, ob, lb;
    if(offset_type==ENCODING_OFFSET_VERS_POS || offset_type==ENCODING_OFFSET_DC_POS)
		off_is_sbytes=1;
    else if(offset_type==ENCODING_OFFSET_START)
		off_is_sbytes=0;
    else {
		printf("wtf, unknown offset_type for reconstruction(%u)\n",offset_type);
		exit(1);
    }

    while(cread(patchf, buff, 1)==1 && *buff != 0) {
    //printf("adding command num(%lu)\n", dcbuff->count+1);
	    if(*buff > 0 && *buff <= 248) {
	        //add command
	        printf("add command type(%u) ", *buff);
	        if(*buff >=247 && *buff <= 248){
        		if (*buff==247)
	        	    lb=2;
				else if (*buff==248)
	        	    lb=4;
				if(cread(patchf, buff, lb)!=lb) {
					printf("shite, error reading.  \n");
					exit(1);
				}
	        	len= readUnsignedBytes(buff, lb);
	        } else
	        	len=*buff;
			printf("len(%lu)\n", len);
	        printf("adding add for len(%lu), dc_pos(%lu)\n", len, ctell(patchf, CSEEK_ABS));
	        DCBufferAddCmd(dcbuff, DC_ADD, ctell(patchf, CSEEK_ABS), len);
	        cseek(patchf, len, CSEEK_CUR);
	    } else if(*buff >= 249 ) {
	        //copy command
            printf("copy command ccom(%u) ", *buff);
	        if(*buff >=249 && *buff <= 251) {
				ob=2;
				if(*buff==249)
				    lb=1;
				else if(*buff==250)
				    lb=2;
				else if(*buff==251)
				    lb=4;
		    } else if (*buff >=252 && *buff <=254) {
				ob=4;
				if(*buff==252)
				    lb=1;
				else if(*buff==253)
				    lb=2;
				else 
				    lb=4;
	    	} else {
				ob=8;
				lb=4;
	    	}
		    if(cread(patchf, buff + 1, lb + ob)!= lb + ob) {
		    	printf("error reading in lb/ob for copy...\n");
		    	exit(1);
		    }
		    //cptr++;
		    if(off_is_sbytes) {
				s_off=readSignedBytes(buff + 1, ob);
				//convertSBytesChar(out_buff + 1, s_off, ob);
		    } else {
				//convertUBytesChar(out_buff + 1, u_off, ob);
				u_off=readUnsignedBytes(buff + 1, ob);
		    }
		    len = readUnsignedBytes(buff + 1 + ob, lb);
		    if(offset_type!=ENCODING_OFFSET_START) {
				if(offset_type==ENCODING_OFFSET_VERS_POS)
				    u_off = ver_pos + s_off;
				else //ENCODING_DC_POS
				    dc_pos = u_off = dc_pos + s_off;
		    }
		    printf("offset(%lu), len(%lu)\n", u_off, len);
		    DCBufferAddCmd(dcbuff, DC_COPY, u_off, len);
		    ver_pos+=len;
		}
    }
    printf("closing command was (%u)\n", *buff);
    printf("cread fh_pos(%lu)\n", ctell(patchf, CSEEK_ABS)); 
    printf("ver_pos(%lu)\n", ver_pos);
    return 0;
}
