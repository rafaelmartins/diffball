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
#include "dcbuffer.h"
#include "switching.h"
#include "cfile.h"
#include "bit-functions.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

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
unsigned long copy_soff_start[] = {
	0x00,
	0x80,
	0x80 + 0x8000,
	0x80 + 0x8000,
	0x80 + 0x8000 + 0x800000};

signed int switchingEncodeDCBuffer(struct CommandBuffer *buffer, 
    unsigned int offset_type, /*unsigned char *ver */
    struct cfile *ver_cfh, /*int fh*/ struct cfile *out_fh)
{
    unsigned char clen;
    unsigned long fh_pos=0;
    signed long s_off;
    unsigned long u_off;
    unsigned long delta_pos=0, dc_pos=0;
    unsigned long copies=0, adds_in_buff=0, adds_in_file=0;
    unsigned int lb, ob;
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
    unsigned int is_neg = 0;
    unsigned long *copy_off_array;

    if(offset_type==ENCODING_OFFSET_DC_POS) {
		copy_off_array = copy_soff_start;
    } else {
		copy_off_array = copy_off_start;
	}
//    printf("commands in buffer(%lu)\n", buffer->count);
    count = buffer->count;
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    total_add_len=0;
    while(count--) {
    	if ((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==DC_ADD){
    		total_add_len += buffer->lb_tail->len;
    	}
    	DCBufferIncr(buffer);
    }
    convertUBytesChar(out_buff, total_add_len, 4);
    cwrite(out_fh, out_buff, 4);
    delta_pos += 4;
    count = buffer->count;
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    while(count--) {
    	if ((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==DC_ADD){
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
    printf("output add block, len(%lu)\n", delta_pos);
    //convertUBytesChar(out_buff, 0, 1);
    //cwrite(out_fh, out_buff, 1);
    //delta_pos++;
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    last_com = DC_COPY;
    dc_pos=0;
    count = buffer->count;
    while(count--){
		if(buffer->lb_tail->len==0) {
		    DCBufferIncr(buffer);
		    continue;
		}
		if((*buffer->cb_tail & (1 << buffer->cb_tail_bit))>0) {
	    	type=DC_COPY;
		} else if ((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==0){
		    type=DC_ADD;
		}
		switch(type)
		{
		case DC_ADD:
		    adds_in_buff++;
		    total_add_len += buffer->lb_tail->len;
		    min_add_len = MIN(min_add_len, buffer->lb_tail->len);
		    max_add_len = MAX(max_add_len, buffer->lb_tail->len);
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
		    }
		    temp_len -= add_len_start[temp];
		    //printf("ubits prior(%u): ", out_buff[0]);
		    convertUBitsChar(out_buff, temp_len, lb);
		    //printf("after(%u): ", out_buff[0]);
		    out_buff[0] |= (temp << 6); 
		    cwrite(out_fh, out_buff, temp + 1);
		    printf("writing add, pos(%lu), len(%lu)\n", delta_pos, buffer->lb_tail->len);
		    delta_pos += temp + 1;
		    fh_pos += buffer->lb_tail->len;
		    last_com = DC_ADD;
		    break;
		case DC_COPY:
			if(last_com == DC_COPY) {
				printf("last command was a copy, outputing blank add\n", 
					last_com, DC_COPY);
				out_buff[0]=0;
				cwrite(out_fh, out_buff, 1);
				delta_pos++;
			}
		    copies++;//clean this up.
		    total_copy_len += buffer->lb_tail->len;
		    min_copy_len = MIN(min_copy_len, buffer->lb_tail->len);
		    max_copy_len = MAX(max_copy_len, buffer->lb_tail->len);
		    //yes this is a hack.  but it works.
		    if(offset_type == ENCODING_OFFSET_DC_POS) {
				s_off = buffer->lb_tail->offset - dc_pos;
				//u_off = 2 * (abs(s_off));
		    	u_off = abs(s_off);
		    	printf("off(%lu), dc_pos(%lu), u_off(%lu), s_off(%ld): ", 
				buffer->lb_tail->offset, dc_pos, u_off, s_off);
		    } else {
				u_off = buffer->lb_tail->offset;
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
		    //printf("len(%lu) falls into cat(%u) for copy_len\n", temp_len, temp);
		    temp_len -= copy_len_start[temp];
		    convertUBitsChar(out_buff, temp_len, lb);
		    //out_buff[0] = (temp_len >> (temp * 8)) & 0x0f;
		    //printf("encoding as %u,%u,%u,%u\n", out_buff[0], out_buff[1], out_buff[2], out_buff[3]);
		    //printf("out[0] was(%u): ", out_buff[0]);
		    out_buff[0] |= (temp << 6);
		    //printf("now(%u)\n", out_buff[0]);
		    lb = temp +1;
		    
		    if(u_off >= copy_off_array[3]) {
		    	temp=3;
		    	ob=32;
		    } else if(u_off >= copy_off_array[2]) {
		    	temp=2;
		    	ob=24;
		    } else if (u_off >= copy_off_array[1]) {
		    	temp=1;
		    	ob=16;
		    } else {
		    	temp=0;
		    	ob=8;
		    }
		    out_buff[0] |= (temp << 4);
		    if(offset_type==ENCODING_OFFSET_DC_POS) {
		    	dc_pos += s_off;
				if(temp) {
					if(s_off > 0) { 
						s_off -= copy_off_array[temp];
						is_neg=0;
					} else { 
						printf("s_off(%ld): ", s_off);
						s_off += copy_off_array[temp];
						printf("s_off(%ld): ", s_off);
						is_neg = 1;
					}
				}
				convertSBytesChar(out_buff + lb, s_off, temp + 1);
				if(is_neg) 
					out_buff[lb] |= 0x80;
		    	is_neg=0;
		    } else {
				u_off -= copy_off_array[temp];
				convertUBytesChar(out_buff + lb, u_off, temp + 1);
			} 
			cwrite(out_fh, out_buff, lb + temp + 1);
		    printf("writing copy delta_pos(%lu), fh_pos(%lu), offset(%ld), len(%lu)\n",
		    	delta_pos, fh_pos, 
		    	(offset_type==ENCODING_OFFSET_DC_POS ? s_off : u_off), 
		    	buffer->lb_tail->len);
		    //printf("    copy: ob(%u), lb(%u)\n", ob, lb);
		    fh_pos+=buffer->lb_tail->len;
		    delta_pos += lb + temp + 1;
		    last_com=DC_COPY;
 		}
		DCBufferIncr(buffer);
    }
    convertUBytesChar(out_buff, 0, 2);
    if(last_com==DC_COPY) {
    	cwrite(out_fh, out_buff,1);
    	delta_pos++;
    }
    cwrite(out_fh, out_buff, 2);
    delta_pos+=2;
    printf("Buffer statistics: copies(%lu), adds(%lu)\n    copy ratio=(%f%%), add ratio(%f%%)\n",
	copies, adds_in_buff, ((float)copies)/((float)(copies + adds_in_buff))*100,
	((float)adds_in_buff)/((float)copies + (float)adds_in_buff)*100);
    printf("Buffer statistics: average copy_len(%f), average add_len(%f)\n",
    ((float)total_copy_len)/((float)copies), 
    ((float)total_add_len)/((float)adds_in_buff));
    printf("Buffer statistics: max_copy(%lu), min_copy(%lu), max_add(%lu), min_add(%lu)\n",
	max_copy_len, min_copy_len, max_add_len, min_add_len);
    return 0;
}

signed int switchingReconstructDCBuff(struct cfile *patchf, struct CommandBuffer *dcbuff, 
    unsigned int offset_type) {
    //unsigned char *cptr;
	const unsigned int buff_size = 4096;
    unsigned char buff[buff_size];
	unsigned long int len;
    unsigned long ver_pos=0, dc_pos=0;
    unsigned long int u_off;
    signed long int s_off;
    unsigned int last_com;
    unsigned long add_off, com_start;
    unsigned int off_is_sbytes, ob, lb;
    unsigned int end_of_patch =0;
    unsigned long *copy_off_array;
    if(offset_type==ENCODING_OFFSET_DC_POS) {
    	printf("using ENCODING_OFFSET_DC_POS\n");
       	copy_off_array = copy_soff_start;
    } else if(offset_type==ENCODING_OFFSET_START) {
		printf("using ENCODING_OFFSET_START\n");
		copy_off_array = copy_off_start;
	} else { 
		printf("wtf, unknown offset_type for reconstruction(%u)\n",offset_type);
		exit(1);
    }
    dc_pos=0;
    printf("starting pos=%lu\n", ctell(patchf, CSEEK_ABS));
    cread(patchf, buff, 4);
    com_start = readUnsignedBytes(buff, 4);
    cseek(patchf, com_start, CSEEK_CUR);
    add_off=4;
	last_com=DC_COPY;
	printf("add data block size(%lu), starting commands at pos(%lu)\n", com_start,
		ctell(patchf, CSEEK_ABS));
    while(cread(patchf, buff, 1)==1 && end_of_patch==0) {
    	printf("processing(%u) at pos(%lu): ", buff[0], ctell(patchf, CSEEK_ABS) -1);
	    if(last_com != DC_ADD) {
	    	lb = (buff[0] >> 6) & 0x3;
	    	len = buff[0] & 0x3f;
	    	if(lb) {
	    		cread(patchf, buff, lb);
	    		len = (len << (lb * 8)) + readUnsignedBytes(buff, lb);
	    		len += add_len_start[lb];
	    	}
	    	if(len) {
	    		DCBufferAddCmd(dcbuff, DC_ADD, add_off, len);
	    		add_off += len;
	    	}
	    	last_com = DC_ADD;
	    	printf("add len(%lu)\n", len);
	    } else if(last_com != DC_COPY) {
	    	lb = (buff[0] >> 6) & 0x3;
	    	ob = (buff[0] >> 4) & 0x3;
	    	//printf("lb(%u), len(%lu): ", lb, len);
	    	len = buff[0] & 0x0f;
	    	if(lb) {
	    		cread(patchf, buff, lb);
	    		len = (len << (lb * 8)) + readUnsignedBytes(buff, lb);
	    		//printf("adding(%lu): ", copy_len_start[lb]);
	    		len += copy_len_start[lb];
	    	}
	    	//printf("len now(%lu): ", len);
	    	printf("ob(%u): ", ob);
	    	cread(patchf, buff, ob + 1);
	    	if (offset_type == ENCODING_OFFSET_DC_POS) {
	    		s_off = readSignedBytes(buff, ob + 1);
			// positive or negative 0?  Yes, for this, there is a difference... 
	    		if(buff[0] & 0x80) {
	    			s_off -= copy_off_array[ob];
	    		} else {
	    			s_off += copy_off_array[ob];
	    		}
				u_off = dc_pos + s_off;
				printf("u_off(%lu), dc_pos(%lu), s_off(%ld): ", 
					u_off, dc_pos, s_off);
				dc_pos = u_off;
	    	} else {
	    		u_off = readUnsignedBytes(buff, ob + 1);
	    		u_off += copy_off_start[ob];
	    	}
	    	if(lb==0 && ob==0 && len==0 && u_off==0) {
	    		printf("zero length, zero offset copy found.\n");
	    		end_of_patch=1;
	    		continue;
	    	}
	    	if(len)
		    	DCBufferAddCmd(dcbuff, DC_COPY, u_off, len);
	    	last_com = DC_COPY;
	    	printf("copy off(%ld), len(%lu)\n", u_off, len);
	    }
	}
    printf("closing command was (%u)\n", *buff);
    printf("cread fh_pos(%lu)\n", ctell(patchf, CSEEK_ABS)); 
    printf("ver_pos(%lu)\n", ver_pos);
    
	return 0;    
}

