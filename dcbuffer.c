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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "dcbuffer.h"
#include "cfile.h"
#include "bit-functions.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

unsigned long inline get_current_command_type(CommandBuffer *buff) {
	return ((*buff->cb_tail >> buff->cb_tail_bit) & 0x01);
}

void updateDCCopyStats(DCStats *stats, signed long pos_offset, signed long dc_offset, unsigned long len)
{
    stats->copy_count++;
    stats->copy_pos_offset_bytes[MAX( MIN(signedBytesNeeded(pos_offset) - 1, 0), 5)]++;
    stats->copy_rel_offset_bytes[MAX(MIN(signedBytesNeeded(dc_offset) -1, 0), 5)]++;
    stats->copy_len_bytes[MAX(MIN(unsignedBytesNeeded(len)-1, 0),5)]++;
}

void updateDCAddStats(DCStats *stats, unsigned long len)
{
    stats->add_count++;
    
}

void undoDCCopyStats(DCStats *stats, signed long pos_offset, unsigned long len)
{
    stats->copy_count++;
    stats->copy_pos_offset_bytes[MAX( MIN(signedBytesNeeded(pos_offset) - 1, 0), 5)]--;
//    stats->copy_rel_offset_bytes[MAX(MIN(signedBytesNeeded(dc_offset) -1, 0), 5)]--;
    stats->copy_len_bytes[MAX(MIN(unsignedBytesNeeded(len)-1, 0),5)]--;
}

void undoDCAddStats(DCStats *stats, unsigned long len)
{
    stats->add_count--;
    
}

void DCBufferTruncate(CommandBuffer *buffer, unsigned long len)
{
    //get the tail to an actual node.
    DCBufferDecr(buffer);
    //printf("truncation: \n");
    while(len) {
		/* should that be less then or equal? */
		if (buffer->lb_tail->len <= len) {
		    len -= buffer->lb_tail->len;
//		    printf("    whole removal of type(%u), offset(%lu), len(%lu)\n",
//			(*buffer->cb_tail & (1 << buffer->cb_tail_bit)) >> buffer->cb_tail_bit, buffer->lb_tail->offset,
//			buffer->lb_tail->len);
		    DCBufferDecr(buffer);
		    buffer->buffer_count--;
		} else {
//		    printf("    partial adjust of type(%u), offset(%lu), len(%lu) is now len(%lu)\n",
//			(*buffer->cb_tail & (1 << buffer->cb_tail_bit))>buffer->cb_tail_bit, buffer->lb_tail->offset,
//			buffer->lb_tail->len, buffer->lb_tail->len - len);
		    buffer->lb_tail->len -= len;
		    len=0;
		}
    }
    DCBufferIncr(buffer);
}


void DCBufferIncr(CommandBuffer *buffer)
{
    buffer->lb_tail = (buffer->lb_end==buffer->lb_tail) ?
	 buffer->lb_start : buffer->lb_tail + 1;
    if (buffer->cb_tail_bit >= 7) {
	buffer->cb_tail_bit = 0;
	buffer->cb_tail = (buffer->cb_tail == buffer->cb_end) ? buffer->cb_start : buffer->cb_tail + 1;
    } else {
	buffer->cb_tail_bit++;
    }
}

void DCBufferDecr(CommandBuffer *buffer)
{
    buffer->lb_tail--;
    if (buffer->cb_tail_bit != 0) {
	buffer->cb_tail_bit--;
    } else {
	buffer->cb_tail = (buffer->cb_tail == buffer->cb_start) ? buffer->cb_end : buffer->cb_tail - 1;
	buffer->cb_tail_bit=7;
    }
}

void DCBufferAddCmd(CommandBuffer *buffer, int type, unsigned long offset, unsigned long len)
{
    if(buffer->buffer_count == buffer->buffer_size) {
		printf("shite, buffer full.\n");
		exit(EXIT_FAILURE);
    }
    buffer->lb_tail->offset = offset;
    buffer->lb_tail->len = len;
    if (type==DC_ADD)
		*buffer->cb_tail &= ~(1 << buffer->cb_tail_bit);
    else
		*buffer->cb_tail |= (1 << buffer->cb_tail_bit);
    buffer->buffer_count++;
    DCBufferIncr(buffer);
}

void DCBufferCollapseAdds(CommandBuffer *buffer)
{
	unsigned long count, *plen;
	unsigned int continued_add;
	count = buffer->buffer_count;
	buffer->lb_tail = buffer->lb_start;
	buffer->cb_tail = buffer->cb_head;
	buffer->cb_tail_bit = buffer->cb_head_bit;
	continued_add=0;
	plen = NULL;
	while(count--) {
	    if((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==DC_ADD) {
		if(continued_add) {
		    *plen += buffer->lb_tail->len;
		    buffer->lb_tail->len = 0;
		} else {
		    continued_add = 1;
		    plen = &buffer->lb_tail->len;
		}
	    } else {
		continued_add=0;
	    }
	    DCBufferIncr(buffer);
	}
}

unsigned long 
DCBufferReset(CommandBuffer *buffer)
{
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    return buffer->buffer_count;
}

void DCBufferInit(CommandBuffer *buffer, unsigned long buffer_size, 
    unsigned long src_size, unsigned long ver_size)
{
    buffer->buffer_count=0;
    buffer->src_size = src_size;
    buffer->ver_size = ver_size;
    buffer_size = (buffer_size > 0 ? (buffer_size/8) : 0) + 1;
    buffer->buffer_size = buffer_size * 8;
    if((buffer->cb_start = (char *)malloc(buffer_size))==NULL){
	perror("shite, malloc failed\n");
	exit(EXIT_FAILURE);
    }
    buffer->cb_head = buffer->cb_tail = buffer->cb_start;
    buffer->cb_end = buffer->cb_start + buffer_size - 1;
    buffer->cb_head_bit = buffer->cb_tail_bit = 0;
    if((buffer->lb_start = (DCLoc *)malloc(sizeof(DCLoc) * 
	buffer->buffer_size))==NULL){
	
	perror("shite, malloc failed\n");
	exit(EXIT_FAILURE);
    }
    buffer->lb_head = buffer->lb_tail = buffer->lb_start;
    buffer->lb_end = buffer->lb_start + buffer->buffer_size - 1;
}

