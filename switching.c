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
#include "switching.h"
//#include "cfile.h"
#include "bit-functions.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

signed int switchingEncodeDCBuffer(struct CommandBuffer *buffer, 
    unsigned int offset_type, /*unsigned char *ver */
    struct cfile *ver_cfh, /*int fh*/ struct cfile *out_fh)
{
    unsigned char /* *ptr,*/ clen;
    unsigned long fh_pos=0;
    signed long s_off;
    unsigned long u_off;
    unsigned long delta_pos=0, dc_pos=0;
    unsigned long copies=0, adds_in_buff=0, adds_in_file=0;
    unsigned int lb, ob, off_is_sbytes;
    unsigned char type, out_buff[256];
    unsigned int  add_buff_size=512;
    unsigned char add_buff[add_buff_size];
    unsigned int  bytes_read=0, temp=0;
    unsigned long  bytes_wrote=0;
    unsigned long count, temp_len;
    unsigned long total_add_len=0, total_copy_len=0;
    unsigned long max_add_len=0, max_copy_len=0;
    unsigned long min_add_len=0xffffffff, min_copy_len=0xffffffff;
    unsigned int last_com;
    
    unsigned long add_len_start[] = {
    	0x0,
    	0x40, 
    	0x40 + 0x4000, 
    	0x40 + 0x4000 + 0x400000}; 
    	//0x40 + 0x4000 + 0x400000 + 0x40000000};
    unsigned long copy_len_start[] = {
    	0x0,
    	0x10, 
    	0x10 + 0x1000, 
    	0x10 + 0x1000 + 0x100000}; 
    	//0x10 + 0x1000 + 0x100000 + 0x10000000};
    unsigned long copy_off_start[] = {
    	0x00,
    	0x100,
    	0x100 + 0x10000,
    	0x100 + 0x10000 + 0x1000000};
    
    if(offset_type==ENCODING_OFFSET_VERS_POS || offset_type==ENCODING_OFFSET_DC_POS)
		off_is_sbytes=1;
    else
		off_is_sbytes=0;
    printf("commands in buffer(%lu)\n", buffer->count);
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    //convertUBytesChar(out_buff, GDIFF_MAGIC, GDIFF_MAGIC_LEN);
    //cwrite(out_fh, out_buff, GDIFF_MAGIC_LEN);
    /*if(offset_type==ENCODING_OFFSET_START)
    	convertUBytesChar(out_buff, GDIFF_VER4, GDIFF_VER_LEN);
    else if(offset_type==ENCODING_OFFSET_VERS_POS)
		convertUBytesChar(out_buff, GDIFF_VER5, GDIFF_VER_LEN);
    else if(offset_type==ENCODING_OFFSET_DC_POS)
		convertUBytesChar(out_buff, GDIFF_VER6, GDIFF_VER_LEN);
    else {
		printf("wtf, gdiff doesn't know offset_type(%u). bug.\n",offset_type);
		exit(1);
    }*/
    //cwrite(out_fh, out_buff, GDIFF_VER_LEN);
	count=buffer->count;
    last_com = DC_COPY;
    while(buffer->count--){
		if((*buffer->cb_tail & (1 << buffer->cb_tail_bit))>0) {
	    	//ptr=ver + buffer->lb_tail->offset;
	    	type=DC_COPY;
		} else if ((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==0){
		    type=DC_ADD;
		}
		switch(type)
		{
		case DC_ADD:
		    printf("add command, delta_pos(%lu), fh_pos(%lu), len(%lu), ",
				delta_pos, fh_pos, buffer->lb_tail->len);
		    adds_in_buff++;
		    total_add_len += buffer->lb_tail->len;
		    min_add_len = MIN(min_add_len, buffer->lb_tail->len);
		    max_add_len = MAX(max_add_len, buffer->lb_tail->len);
		    //convertUBytesChar(out_buff, buffer->lb_tail->len, 4);
		    //cwrite(out_fh, out_buff, 4);
		    //delta_pos+=4;
		    /*lb=unsignedBitsNeeded(buffer->lb_tail->len);
		    if(lb <=6) {
		    	convertUBitsChar(out_buff, buffer->lb_tail->len, 6);
		    	//out_buff[0] &= 0x3f;
		    	temp=1;
		    } else if(lb <=14) {
		    	convertUBitsChar(out_buff, buffer->lb_tail->len, 14);
		    	out_buff[0] |= 0x40;
		    	temp=2;
		    } else if(lb <= 22) {
		    	convertUBitsChar(out_buff, buffer->lb_tail->len, 22);
		    	out_buff[0] |= 0x80;
		    	temp=3;
		    } else if(lb <= 30) {
		    	convertUBitsChar(out_buff, buffer->lb_tail->len, 30);
		    	out_buff[0] |= 0xc0;
		    	temp=4;
		    } else {
		    	printf("eh?  too large... lb=%u\n", lb);
		    	exit(1);
		    } */
		    temp_len = buffer->lb_tail->len;
		    if(temp_len >= add_len_start[3]) {
		    	temp=3;
		    	lb=30;
		    } else if (temp_len >= add_len_start[2]) {
		    	temp=2;
		    	lb=22;
		    } else if (temp_len >= add_len_start[1]) {
		    	temp=1;
		    	lb=14;
		    } else {
		    	temp=0;
		    	lb=6;
		    }/*
		    if(temp_len < add_len_limit[0]) { //2**7 -1
		    	temp=0;
		    	lb=6;
		    } else if (temp_len < add_len_limit[1]) {
		    	temp=1;
		    	lb=14;
		    } else if (temp_len < add_len_limit[2]) {
		    	temp=2;
		    	lb=22;
		    } else if (temp_len < add_len_limit[3]) {
		    	temp=3;
		    	lb=30;
		    } else {
		    	printf("fuck, too large- len(%lu)\n");
		    	exit(1);
		    }*/ 
		    temp_len -= add_len_start[temp];
		    convertUBitsChar(out_buff, temp_len, lb);
		    out_buff[0] |= (temp << 6); 
		    cwrite(out_fh, out_buff, temp + 1);
		    printf("    add command, lb(%u)\n", lb);
		    delta_pos += temp + 1;
		    /*u_off=buffer->lb_tail->len;
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
		    }*/
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
		    last_com = DC_ADD;
		    break;
		case DC_COPY:
			if(last_com == DC_COPY) {
				out_buff[0]=0;
				cwrite(out_fh, out_buff, 1);
				delta_pos++;
			}
		    copies++;//clean this up.
		    total_copy_len += buffer->lb_tail->len;
		    min_copy_len = MIN(min_copy_len, buffer->lb_tail->len);
		    max_copy_len = MAX(max_copy_len, buffer->lb_tail->len);
		    if(off_is_sbytes) {
				if(offset_type==ENCODING_OFFSET_VERS_POS)
				    s_off = (signed long)buffer->lb_tail->offset - (signed long)fh_pos;
				else if(offset_type==ENCODING_OFFSET_DC_POS)
				    s_off = (signed long)buffer->lb_tail->offset - (signed long)dc_pos;		
				//ob=signedBytesNeeded(s_off);
				u_off = 2 * (abs(s_off));
		    } else {
				u_off = buffer->lb_tail->offset;
				//ob=unsignedBytesNeeded(u_off);
		    }
		    temp_len = buffer->lb_tail->len;
		    if(temp_len >= copy_len_start[3]) {
		    	temp=3;
		    	lb=28;
		    } else if (temp_len >= copy_len_start[2]) {
		    	temp=2;
		    	lb=20;
		    } else if (temp_len >= copy_len_start[1]) {
		    	temp=1;
		    	lb=12;
		    } else {
		    	temp=0;
		    	lb=4;
		    }
		    temp_len -= copy_len_start[temp];
		    convertUBitsChar(out_buff, temp_len, lb);
		    out_buff[0] |= (temp << 6);
		    lb = temp +1;
		    
		    if(u_off >= copy_off_start[3]) {
		    	temp=3;
		    	ob=32;
		    } else if(u_off >= copy_off_start[2]) {
		    	temp=2;
		    	ob=24;
		    } else if (u_off >= copy_off_start[1]) {
		    	temp=1;
		    	ob=16;
		    } else {
		    	temp=0;
		    	ob=8;
		    }
		    
		    if(off_is_sbytes) {
				if(temp)
					s_off -= (copy_off_start[temp]/2);
				convertSBytesChar(out_buff + lb, s_off, ob);
		    	dc_pos += s_off;
		    } else {
		    	//printf("u_off prior(%lu), temp(%u):", u_off, temp);
				u_off -= copy_off_start[temp];
				//printf(" after(%lu)\n", u_off);
				convertUBytesChar(out_buff + lb, u_off, ob);
				//dc_pos = u_off;
			} 
			cwrite(out_fh, out_buff, lb + temp + 1);
			//delta_pos += temp + lb + 1;
		    
		    /*if(lb> INT_BYTE_COUNT) {
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
		    */
		    //convertUBytesChar(out_buff, buffer->lb_tail->offset, 4);
		    //s_off = (signed long)buffer->lb_tail->offset - (signed long)dc_pos;
		    //convertSBytesChar(out_buff + 4, s_off, 4);
		    //clen=8;
		    printf("writing copy delta_pos(%lu), fh_pos(%lu), offset(%ld), len(%lu)\n",
			delta_pos, fh_pos, (off_is_sbytes ? s_off: u_off), buffer->lb_tail->len);
		    printf("    copy: ob(%u), lb(%u)\n", ob, lb);
		    //if(cwrite(out_fh, out_buff, clen)!=clen) {
			//	printf("shite, couldn't write copy command. eh?\n");
			//	exit(1);
		    //}
		    fh_pos+=buffer->lb_tail->len;
		    delta_pos += temp + ob + 1;
 		}
		DCBufferIncr(buffer);
    }
    convertUBytesChar(out_buff, 0, 1);
    cwrite(out_fh, out_buff, 1);
    delta_pos++;
    printf("wrote commands, now writing add data starting at(%lu)\n", delta_pos);
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
    delta_pos++;
    printf("Buffer statistics: copies(%lu), adds(%lu)\n    copy ratio=(%f%%), add ratio(%f%%)\n",
	copies, adds_in_buff, ((float)copies)/((float)(copies + adds_in_buff))*100,
	((float)adds_in_buff)/((float)copies + (float)adds_in_buff)*100);
    printf("Buffer statistics: average copy_len(%f), average add_len(%f)\n",
    ((float)total_copy_len)/((float)copies), 
    ((float)total_add_len)/((float)adds_in_buff));
    printf("Buffer statistics: max_copy(%lu), min_copy(%lu), max_add(%lu), min_add(%lu)\n",
	max_copy_len, min_copy_len, max_add_len, min_add_len);
    //printf("adds in file(%lu), average # of commands per add(%f)\n", adds_in_file,
	//((float)adds_in_file)/((float)(adds_in_buff)));
    //ahem.  better error handling/returning needed. in time, in time...
    return 0;
}


