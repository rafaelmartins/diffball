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
#include <errno.h>
#include "dcbuffer.h"
#include "cfile.h"
#include "bit-functions.h"
#include "defs.h"

extern unsigned int verbosity;

unsigned long 
inline current_command_type(CommandBuffer *buff)
{
    if(DCBUFFER_FULL_TYPE == buff->DCBtype) {
        return ((*buff->DCB.full.cb_tail >> buff->DCB.full.cb_tail_bit) & 0x1);
    }
    return 0;
}

DCLoc *
current_DCLoc(CommandBuffer *buff)
{
    assert(buff->DCBtype == DCBUFFER_FULL_TYPE);
    return buff->DCB.full.lb_tail;
}

void 
DCBufferTruncate(CommandBuffer *buffer, unsigned long len)
{
    //get the tail to an actual node.
    DCBufferDecr(buffer);
    //v2printf("truncation: \n");
    while(len) {
	/* should that be less then or equal? */
	if (buffer->DCB.full.lb_tail->len <= len) {
	    len -= buffer->DCB.full.lb_tail->len;
//		    v2printf("    whole removal of type(%u), offset(%lu), len(%lu)\n",
//			(*buffer->cb_tail & (1 << buffer->cb_tail_bit)) >> buffer->cb_tail_bit, buffer->lb_tail->offset,
//			buffer->lb_tail->len);
	    DCBufferDecr(buffer);
	    buffer->DCB.full.buffer_count--;
	} else {
//		    v2printf("    partial adjust of type(%u), offset(%lu), len(%lu) is now len(%lu)\n",
//			(*buffer->cb_tail & (1 << buffer->cb_tail_bit))>buffer->cb_tail_bit, buffer->lb_tail->offset,
//			buffer->lb_tail->len, buffer->lb_tail->len - len);
	    buffer->DCB.full.lb_tail->len -= len;
	    len=0;
	}
    }
    DCBufferIncr(buffer);
}


void 
DCBufferIncr(CommandBuffer *buffer)
{
    assert(DCBUFFER_FULL_TYPE==buffer->DCBtype);
    buffer->DCB.full.lb_tail = (buffer->DCB.full.lb_end == 
	buffer->DCB.full.lb_tail) ?
    buffer->DCB.full.lb_start : buffer->DCB.full.lb_tail + 1;
    if (buffer->DCB.full.cb_tail_bit >= 7) {
	buffer->DCB.full.cb_tail_bit = 0;
	buffer->DCB.full.cb_tail = (buffer->DCB.full.cb_tail == 
	    buffer->DCB.full.cb_end) ? buffer->DCB.full.cb_start : 
	    buffer->DCB.full.cb_tail + 1;
    } else {
	buffer->DCB.full.cb_tail_bit++;
    }
}

void 
DCBufferDecr(CommandBuffer *buffer)
{
    assert(DCBUFFER_FULL_TYPE == buffer->DCBtype);
    buffer->DCB.full.lb_tail--;
    if (buffer->DCB.full.cb_tail_bit != 0) {
	buffer->DCB.full.cb_tail_bit--;
    } else {
	buffer->DCB.full.cb_tail = (buffer->DCB.full.cb_tail == 
	    buffer->DCB.full.cb_start) ? buffer->DCB.full.cb_end : 
	    buffer->DCB.full.cb_tail - 1;
	buffer->DCB.full.cb_tail_bit=7;
    }
}

void 
DCBufferAddCmd(CommandBuffer *buffer, int type, unsigned long offset, 
    unsigned long len)
{
    if(buffer->DCBtype==DCBUFFER_FULL_TYPE) {
	if(buffer->DCB.full.lb_tail == buffer->DCB.full.lb_end) {
	    v1printf("resizing command buffer from %lu to %lu\n", 
		buffer->DCB.full.buffer_size, buffer->DCB.full.buffer_size * 2);
	    if((buffer->DCB.full.cb_start = (char *)realloc(
		buffer->DCB.full.cb_start, buffer->DCB.full.buffer_size /4 ))
		==NULL) {

		v0printf("resizing command buffer failed, exiting\n");
		exit(EXIT_FAILURE);
	    } else if((buffer->DCB.full.lb_start = 
		(DCLoc *)realloc(buffer->DCB.full.lb_start, 
		buffer->DCB.full.buffer_size * 2 * sizeof(DCLoc)) )==NULL) {
		v0printf("resizing command buffer failed, exiting\n");
		exit(EXIT_FAILURE);
	    }
	    buffer->DCB.full.buffer_size *= 2;
	    buffer->DCB.full.cb_head = buffer->DCB.full.cb_start;
	    buffer->DCB.full.lb_head = buffer->DCB.full.lb_start;
	    buffer->DCB.full.lb_tail = buffer->DCB.full.lb_start + 
		buffer->DCB.full.buffer_count;
	    buffer->DCB.full.cb_tail = buffer->DCB.full.cb_start + 
		(buffer->DCB.full.buffer_count/8);
	    buffer->DCB.full.lb_end = buffer->DCB.full.lb_start + 
		buffer->DCB.full.buffer_size -1;
	    buffer->DCB.full.cb_end = buffer->DCB.full.cb_start + 
		(buffer->DCB.full.buffer_size/8) -1;
	}
	buffer->DCB.full.lb_tail->offset = offset;
	buffer->DCB.full.lb_tail->len = len;
	if (type==DC_ADD)
	    *buffer->DCB.full.cb_tail &= ~(1 << buffer->DCB.full.cb_tail_bit);
	else
	    *buffer->DCB.full.cb_tail |= (1 << buffer->DCB.full.cb_tail_bit);
	buffer->DCB.full.buffer_count++;
	DCBufferIncr(buffer);
    }
}

void 
DCBufferCollapseAdds(CommandBuffer *buffer)
{
    unsigned long count, *plen;
    unsigned int continued_add;
    count = buffer->DCB.full.buffer_count;
    buffer->DCB.full.lb_tail = buffer->DCB.full.lb_start;
    buffer->DCB.full.cb_tail = buffer->DCB.full.cb_head;
    buffer->DCB.full.cb_tail_bit = buffer->DCB.full.cb_head_bit;
    continued_add=0;
    plen = NULL;
    if(buffer->DCBtype==DCBUFFER_FULL_TYPE){
	while(count--) {
	    if((*buffer->DCB.full.cb_tail & (1 << 
		buffer->DCB.full.cb_tail_bit))==DC_ADD) {
		if(continued_add) {
		    *plen += buffer->DCB.full.lb_tail->len;
		    buffer->DCB.full.lb_tail->len = 0;
		} else {
		    continued_add = 1;
		    plen = &buffer->DCB.full.lb_tail->len;
		}
	    } else {
		continued_add=0;
	    }
	    DCBufferIncr(buffer);
	}
    }
}

unsigned long 
DCBufferReset(CommandBuffer *buffer)
{
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	buffer->DCB.full.lb_tail = buffer->DCB.full.lb_start;
	buffer->DCB.full.cb_tail = buffer->DCB.full.cb_head;
	buffer->DCB.full.cb_tail_bit = buffer->DCB.full.cb_head_bit;
	return buffer->DCB.full.buffer_count;
    }
    return 0;
}

void DCBufferFree(CommandBuffer *buffer)
{
    if(buffer->flags & ADD_CFH_FREE_FLAG)
	free(buffer->add_cfh);
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	free(buffer->DCB.full.cb_start);
	free(buffer->DCB.full.lb_start);
    }
}

void DCBufferInit(CommandBuffer *buffer, unsigned long buffer_size, 
    unsigned long src_size, unsigned long ver_size, unsigned char type)
{
//    buffer->buffer_count=0;
    buffer->flags =0;
    buffer->src_size = src_size;
    buffer->ver_size = ver_size;
    buffer->DCBtype = type;
    if(type == DCBUFFER_FULL_TYPE) {
	buffer->DCB.full.buffer_count = 0;
	buffer_size = (buffer_size > 0 ? (buffer_size/8) : 0) + 1;
	buffer->DCB.full.buffer_size = buffer_size * 8;
	/* non-intuitive, but note I'm using *buffer_size* rather then 
	  buffer->DCB.full.buffer_size.  it makes one hell of a difference. */
	if((buffer->DCB.full.cb_start =
	    (unsigned char *)malloc(buffer_size))==NULL){
	    perror("shite, malloc failed\n");
	    exit(EXIT_FAILURE);
	}
	buffer->DCB.full.cb_head = buffer->DCB.full.cb_tail = 
	    buffer->DCB.full.cb_start;
	buffer->DCB.full.cb_end = buffer->DCB.full.cb_start + buffer_size - 1;
	buffer->DCB.full.cb_head_bit = buffer->DCB.full.cb_tail_bit = 0;
	if((buffer->DCB.full.lb_start = (DCLoc *)malloc(sizeof(DCLoc) * 
	    buffer->DCB.full.buffer_size))==NULL){
	    perror("shite, malloc failed\n");
	    exit(EXIT_FAILURE);
	}
        buffer->DCB.full.lb_head = buffer->DCB.full.lb_tail = 
	    buffer->DCB.full.lb_start;
	buffer->DCB.full.lb_end = buffer->DCB.full.lb_start + 
	    buffer->DCB.full.buffer_size - 1;
    }
}

