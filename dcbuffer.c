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

void
DCB_get_next_command(CommandBuffer *buff, DCommand *dc)
{
    if(DCBUFFER_FULL_TYPE == buff->DCBtype) {
	dc->type = current_command_type(buff);
	dc->loc.offset = buff->DCB.full.lb_tail->offset;
	dc->loc.len = buff->DCB.full.lb_tail->len;
	DCBufferIncr(buff);
    } else if (DCBUFFER_MATCHES_TYPE == buff->DCBtype) {
	assert(buff->DCB.matches.buff_count > 
	    (buff->DCB.matches.cur - buff->DCB.matches.buff));
	assert(buff->reconstruct_pos != buff->ver_size);
	if(buff->reconstruct_pos == buff->DCB.matches.cur->ver_pos) {
	    dc->type = DC_COPY;
	    dc->loc.offset = buff->DCB.matches.cur->src_pos;
	    dc->loc.len = buff->DCB.matches.cur->len;
	    DCBufferIncr(buff);
	} else {
	    dc->type = DC_ADD;
	    dc->loc.offset = buff->reconstruct_pos;
	    dc->loc.len= buff->DCB.matches.cur->ver_pos - buff->reconstruct_pos;
	}
	buff->reconstruct_pos += dc->loc.len;
    }
}

void 
DCB_truncate(CommandBuffer *buffer, unsigned long len)
{
    unsigned long trunc_pos;
    //get the tail to an actual node.
    DCBufferDecr(buffer);
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	buffer->reconstruct_pos -= len;
	while(len) {
	    if (buffer->DCB.full.lb_tail->len <= len) {
		len -= buffer->DCB.full.lb_tail->len;
		DCBufferDecr(buffer);
		buffer->DCB.full.buffer_count--;
	    } else {
		buffer->DCB.full.lb_tail->len -= len;
		len=0;
	    }
	}
    } else if (DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	assert(buffer->DCB.matches.buff_count > 0);
	assert(buffer->DCB.matches.cur->ver_pos + 
	    buffer->DCB.matches.cur->len - len >= 0);
	trunc_pos = buffer->DCB.matches.cur->ver_pos + 
	    buffer->DCB.matches.cur->len - len;
	//buffer->reconstruct_pos = trunc_pos;
	while(trunc_pos < buffer->DCB.matches.cur->ver_pos + 
	    buffer->DCB.matches.cur->len) {
	    if(buffer->DCB.matches.cur->ver_pos >= trunc_pos) {
		DCBufferDecr(buffer);
		buffer->DCB.matches.buff_count--;
	    } else {
		buffer->DCB.matches.cur->len = trunc_pos - 
		    buffer->DCB.matches.cur->ver_pos;
	    }
	}
	buffer->reconstruct_pos = buffer->DCB.matches.cur->ver_pos + 
	    buffer->DCB.matches.cur->len;
    }
    DCBufferIncr(buffer);	
}


void 
DCBufferIncr(CommandBuffer *buffer)
{
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
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
	assert(buffer->DCB.full.lb_head != buffer->DCB.full.lb_tail);
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	assert(buffer->DCB.matches.cur - buffer->DCB.matches.buff < 
	    buffer->DCB.matches.buff_size);
	    buffer->DCB.matches.cur++;
    }
}

void 
DCBufferDecr(CommandBuffer *buffer)
{
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	buffer->DCB.full.lb_tail--;
	if (buffer->DCB.full.cb_tail_bit != 0) {
	    buffer->DCB.full.cb_tail_bit--;
	} else {
	    buffer->DCB.full.cb_tail = (buffer->DCB.full.cb_tail == 
		buffer->DCB.full.cb_start) ? buffer->DCB.full.cb_end : 
	        buffer->DCB.full.cb_tail - 1;
	    buffer->DCB.full.cb_tail_bit=7;
	}
    } else if (DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	if(buffer->DCB.matches.cur != buffer->DCB.matches.buff) {
	    buffer->DCB.matches.cur--;
	/*    if(buffer->DCB.matches.cur->ver_pos + buffer->DCB.matches.cur->len
		== buffer->reconstruct_pos) {
		buffer->reconstruct_pos = buffer->DCB.matches.cur->ver_pos;
	    } else {
		buffer->reconstruct_pos = buffer->DCB.matches.cur->ver_pos + 
		    buffer->DCB.matches.cur->len;
		buffer->DCB.matches.cur++;
	    }*/
	
	} else {
	    buffer->reconstruct_pos = 0;
	}
    }
}

void 
DCB_add_add(CommandBuffer *buffer, unsigned long ver_pos, 
    unsigned long len)
{
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	if(buffer->DCB.full.lb_tail == buffer->DCB.full.lb_end)
	    DCB_resize_full(buffer);
	buffer->DCB.full.lb_tail->offset = ver_pos;
	buffer->DCB.full.lb_tail->len = len;
	*buffer->DCB.full.cb_tail &= ~(1 << buffer->DCB.full.cb_tail_bit);
	buffer->DCB.full.buffer_count++;
	buffer->reconstruct_pos += len;
	DCBufferIncr(buffer);
    }
}

void
DCB_add_copy(CommandBuffer *buffer, unsigned long src_pos, 
    unsigned long ver_pos, unsigned long len)
{
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	if(buffer->DCB.full.lb_tail == buffer->DCB.full.lb_end)
	    DCB_resize_full(buffer);
	buffer->DCB.full.lb_tail->offset = src_pos;
	buffer->DCB.full.lb_tail->len = len;
	*buffer->DCB.full.cb_tail |= (1 << buffer->DCB.full.cb_tail_bit);
	buffer->DCB.full.buffer_count++;
	buffer->reconstruct_pos += len;
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	if(buffer->DCB.matches.buff_count == buffer->DCB.matches.buff_size) {
	    DCB_resize_matches(buffer);
	}
	assert(buffer->DCB.matches.buff_count == 
	    (buffer->DCB.matches.cur - buffer->DCB.matches.buff));
	buffer->DCB.matches.cur->src_pos = src_pos;
	buffer->DCB.matches.cur->ver_pos = ver_pos;
	buffer->DCB.matches.cur->len = len;
	buffer->DCB.matches.buff_count++;
	buffer->reconstruct_pos = ver_pos + len;
    }
    DCBufferIncr(buffer);
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



void
DCBufferReset(CommandBuffer *buffer)
{
    buffer->reconstruct_pos = 0;
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	buffer->DCB.full.lb_tail = buffer->DCB.full.lb_start;
	buffer->DCB.full.cb_tail = buffer->DCB.full.cb_head;
	buffer->DCB.full.cb_tail_bit = buffer->DCB.full.cb_head_bit;
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	buffer->DCB.matches.cur = buffer->DCB.matches.buff;
    }
}

unsigned int
DCB_commands_remain(CommandBuffer *buffer)
{
    unsigned long x;
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	if(buffer->DCB.full.lb_head > buffer->DCB.full.lb_tail) {
	    x=(buffer->DCB.full.buffer_count - (buffer->DCB.full.lb_end - 
	    buffer->DCB.full.lb_head) - (buffer->DCB.full.lb_tail - 
	    buffer->DCB.full.lb_start));
//	    printf("com_remain: head > tail, x=%lu\n", x);
	} else {
	    x= (buffer->DCB.full.buffer_count - (buffer->DCB.full.lb_tail - 
		buffer->DCB.full.lb_head));
//	    printf("com_remain: head <= tail, x=%lu\n", x);
	}
	return x+1 > 1;
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	return buffer->reconstruct_pos != buffer->ver_size;
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
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	free(buffer->DCB.matches.buff);
    }
}

void DCBufferInit(CommandBuffer *buffer, unsigned long buffer_size, 
    unsigned long src_size, unsigned long ver_size, unsigned char type)
{
    buffer->flags =0;
    buffer->src_size = src_size;
    buffer->ver_size = ver_size;
    buffer->reconstruct_pos = 0;
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
    } else if(DCBUFFER_MATCHES_TYPE == type) {
	if((buffer->DCB.matches.buff = (DCLoc_match *)malloc(buffer_size * 
	    sizeof(DCLoc_match)) )==NULL) {
	    perror("shite, malloc failed\n");
	    exit(EXIT_FAILURE);
	}
	buffer->DCB.matches.cur = buffer->DCB.matches.buff;
	buffer->DCB.matches.buff_size = buffer_size;
	buffer->DCB.matches.buff_count = 0;
    }
}

void
DCB_resize_matches(CommandBuffer *buffer)
{
    assert(DCBUFFER_MATCHES_TYPE == buffer->DCBtype);
    v1printf("resizing matches buffer from %lu to %lu\n", 
	buffer->DCB.matches.buff_size, buffer->DCB.matches.buff_size * 2);
    buffer->DCB.matches.buff_size *= 2;
    if((buffer->DCB.matches.buff = (DCLoc_match *)realloc(
	buffer->DCB.matches.buff, buffer->DCB.matches.buff_size * 
	sizeof(DCLoc_match))) == NULL) {
	v0printf("buffer resize failed\n");
	exit(1);
    }
    buffer->DCB.matches.cur = buffer->DCB.matches.buff + 
	buffer->DCB.matches.buff_count;
}

void
DCB_resize_full(CommandBuffer *buffer)
{
    assert(DCBUFFER_FULL_TYPE == buffer->DCBtype);
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
