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
#include "raw.h"
#include "cfile.h"
#include "bit-functions.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

signed int 
rawEncodeDCBuffer(CommandBuffer *buffer, unsigned int offset_type, 
    cfile *ver_cfh, cfile *out_fh)
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
    unsigned long command_count = 0;

    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    total_add_len=0;
    command_count=0;
    count = buffer->buffer_count;
    last_com=DC_COPY;
    if(*buffer->cb_tail & (1 << buffer->cb_tail_bit))
	command_count++;
    while(count--) {
    	if(buffer->lb_tail->len!=0) {
	    if (get_current_command_type(buffer)==DC_ADD) {
    		total_add_len += buffer->lb_tail->len;
		last_com = DC_ADD;
	    } else {
		if(last_com==DC_COPY) {
		    command_count++;
		}
		last_com = DC_COPY;
	    }
	    command_count++;
	}
    	DCBufferIncr(buffer);
    }
    printf("count=%lu, command_count=%lu\n", buffer->buffer_count, command_count);
    writeUBytesBE(out_buff, command_count, 4);
    writeUBytesBE(out_buff + 4, total_add_len, 4);
    cwrite(out_fh, out_buff, 8);
    delta_pos += 8;
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    count = buffer->buffer_count;
    while(count--) {
	if(((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==DC_ADD) &&
	    buffer->lb_tail->len!=0) {
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
    count = buffer->buffer_count;
    last_com = DC_COPY;
    dc_pos=0;
    while(command_count--){
	if(buffer->lb_tail->len > 0) {
	    if(DC_ADD == get_current_command_type(buffer) ) {
		    adds_in_buff++;
		    min_add_len = MIN(min_add_len, buffer->lb_tail->len);
		    max_add_len = MAX(max_add_len, buffer->lb_tail->len);
		    writeUBytesBE(out_buff, buffer->lb_tail->len, 4);
		    cwrite(out_fh, out_buff, 4);
		    printf("writing add, pos(%lu), len(%lu)\n", delta_pos, buffer->lb_tail->len);
		    delta_pos += 4;
		    fh_pos += buffer->lb_tail->len;
		    last_com = DC_ADD;
	    } else {
		    if(DC_COPY==last_com) {
			printf("last command was a copy, outputing blank add\n", 
			    last_com, DC_COPY);
			writeUBytesBE(out_buff, 0, 4);
			cwrite(out_fh, out_buff, 4);
			delta_pos+=4;
		    }
		    copies++;//clean this up.
		    total_copy_len += buffer->lb_tail->len;
		    min_copy_len = MIN(min_copy_len, buffer->lb_tail->len);
		    max_copy_len = MAX(max_copy_len, buffer->lb_tail->len);
		    //yes this is a hack.  but it works.
		    if(offset_type == ENCODING_OFFSET_DC_POS) {
			s_off = buffer->lb_tail->offset - dc_pos;
		    	u_off = abs(s_off);
		    	printf("off(%lu), dc_pos(%lu), u_off(%lu), s_off(%ld): ", 
			    buffer->lb_tail->offset, dc_pos, u_off, s_off);
		    } else {
			u_off = buffer->lb_tail->offset;
		    }
		    writeUBytesBE(out_buff, buffer->lb_tail->len, 4);
		    if(offset_type==ENCODING_OFFSET_DC_POS) {
		    	dc_pos += s_off;
			writeSBytesBE(out_buff + 4, s_off, 4);
		    } else {
			writeUBytesBE(out_buff + 4, u_off, 4);
		    } 
		    cwrite(out_fh, out_buff, 8);
		    printf("writing copy delta_pos(%lu), fh_pos(%lu), offset(%ld), len(%lu)\n",
		    	delta_pos, fh_pos, 
		    	(offset_type==ENCODING_OFFSET_DC_POS ? s_off : u_off), 
		    	buffer->lb_tail->len);
		    fh_pos+=buffer->lb_tail->len;
		    delta_pos += 8;
		    last_com=DC_COPY;
	    }
	}
	DCBufferIncr(buffer);
    }
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

signed int 
rawReconstructDCBuff(cfile *patchf, CommandBuffer *dcbuff, 
    unsigned int offset_type) 
{
    const unsigned int buff_size = 8;
    unsigned char buff[buff_size];
    unsigned long int len;
    unsigned long dc_pos=0;
    unsigned long int u_off, add_off, com_start;
    unsigned int last_com;
    unsigned int off_is_sbytes;
    unsigned long command_count, add_block_len;
    if(offset_type==ENCODING_OFFSET_DC_POS) {
	off_is_sbytes = 1;
    } else {
	off_is_sbytes = 0;
    }
    cread(patchf, buff, 8);
    command_count = readUBytesBE(buff, 4);
    com_start = readUBytesBE(buff + 4, 4);
    cseek(patchf, com_start, CSEEK_CUR);
    dc_pos = 8 + com_start;
    add_off = 8;
    last_com=DC_COPY;
    while(command_count-- && cread(patchf, buff, 4)==4) {
	len = readUBytesBE(buff, 4);
	printf("command(%lu), len(%lu)\n", command_count + 1, len);
	if(last_com==DC_COPY) { //eg it's an add
	    last_com = DC_ADD;
	    if(len) {
		printf("adding len(%lu)\n", len);
		DCBufferAddCmd(dcbuff, DC_ADD, add_off, len);
		add_off += len;
	    }
	} else {
	    last_com = DC_COPY;
	    if(cread(patchf, buff, 4)!=4) {
		abort();
	    }
	    if(len) {
		if(off_is_sbytes) {
		    u_off = dc_pos + readSBytesBE(buff, 4);
		} else {
		    u_off = readUBytesBE(buff, 4);
		}
		printf("copying len(%lu)\n", len);
		DCBufferAddCmd(dcbuff, DC_COPY, u_off, len);
	    }
	}
    }
    return 0;
}

