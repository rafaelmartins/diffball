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
DCB_test_total_copy_len(CommandBuffer *buff)
{
#ifdef DEBUG_DCBUFFER
    unsigned long computed_len = 0;
    DCommand dc;
    DCBufferReset(buff);
    while(DCB_commands_remain(buff)) {
        DCB_get_next_command(buff, &dc);
        if(dc.type==DC_COPY)
            computed_len += dc.loc.len;
    }
    v0printf("dcbuffer test: copy_len(%lu==%lu)\n", buff->total_copy_len,
        computed_len);
    DCBufferReset(buff);
#endif
}

unsigned int
DCB_get_next_gap(CommandBuffer *buff, unsigned long gap_req, DCLoc *dc)
{
    assert(buff->flags & DCB_LLM_FINALIZED);
    dc->len = dc->offset = 0;
    if(buff->DCB.llm.gap_pos == buff->ver_size){ // || buff->DCB.llm.main == NULL) {
	return 0;
    } else if(buff->DCB.llm.gap_pos == 0) {
	if(buff->DCB.llm.main_count==0) {
	    if(gap_req <= buff->ver_size) {
		dc->len = buff->ver_size;
	    }
	    buff->DCB.llm.gap_pos = buff->ver_size;
	    return dc->len != 0;
	} else if(buff->DCB.llm.main_head->ver_pos >= gap_req) {
	    buff->DCB.llm.gap_pos = dc->len = buff->DCB.llm.main_head->ver_pos;
	    return 1;
	}
    }
    if(1==1){
	while(buff->DCB.llm.main != NULL  && 
	   buff->DCB.llm.gap_pos > buff->DCB.llm.main->ver_pos) {
//    v1printf("debug info gap_skew, gap_pos(%lu), main(%u), main->next(%u)\n", 
//	buff->DCB.llm.gap_pos, buff->DCB.llm.main != NULL, 
//	buff->DCB.llm.main != NULL ? buff->DCB.llm.main->next != NULL : 0);
//	v1printf("ver_pos %lu\n", buff->DCB.llm.main->ver_pos);
	    DCBufferIncr(buff);
	}
	while(buff->DCB.llm.main != NULL && dc->len == 0) {
//    v1printf("debug info loop, gap_pos(%lu), main(%u), main->next(%u)\n", 
//	buff->DCB.llm.gap_pos, buff->DCB.llm.main != NULL, 
//	buff->DCB.llm.main != NULL ? buff->DCB.llm.main->next != NULL : 0);
//	v1printf("ver_pos %lu\n", buff->DCB.llm.main->ver_pos);
	    if(buff->DCB.llm.main->next == NULL) {
		buff->DCB.llm.gap_pos = buff->ver_size;
		if(buff->ver_size - LLM_VEND(buff->DCB.llm.main)
		    >= gap_req) {
		    dc->offset = LLM_VEND(buff->DCB.llm.main);
		    dc->len = buff->ver_size - LLM_VEND(buff->DCB.llm.main);
		} else {
		    DCBufferIncr(buff);
		    return 0;
		}
	    } else if(buff->DCB.llm.main->next->ver_pos - 
		LLM_VEND(buff->DCB.llm.main) >= gap_req) {
		dc->offset = LLM_VEND(buff->DCB.llm.main);
		dc->len = buff->DCB.llm.main->next->ver_pos - dc->offset;
		buff->DCB.llm.gap_pos = buff->DCB.llm.main->next->ver_pos;
	    } else {
		DCBufferIncr(buff);
	    }
	}
    }
    if(dc->len != 0) {
	return 1;
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
	assert(buff->reconstruct_pos != buff->ver_size);
	if((buff->DCB.matches.cur - buff->DCB.matches.buff) == 
	    buff->DCB.matches.buff_count) {
	    dc->type = DC_ADD;
	    dc->loc.offset = buff->reconstruct_pos;
	    dc->loc.len = buff->ver_size - buff->reconstruct_pos;
	} else if(buff->reconstruct_pos == buff->DCB.matches.cur->ver_pos) {
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
    } else if(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype) {
	assert(buff->flags & DCB_LLM_FINALIZED);
	if(buff->DCB.llm.main == NULL) {
	    dc->type = DC_ADD;
	    dc->loc.offset = buff->reconstruct_pos;
	    dc->loc.len = buff->ver_size - buff->reconstruct_pos;
	} else if(buff->reconstruct_pos == buff->DCB.llm.main->ver_pos) {
	    dc->type = DC_COPY;
	    dc->loc.offset = buff->DCB.llm.main->src_pos;
	    dc->loc.len = buff->DCB.llm.main->len;
	    DCBufferIncr(buff);
	} else {
	    dc->type = DC_ADD;
	    dc->loc.offset = buff->reconstruct_pos;
	    dc->loc.len = buff->DCB.llm.main->ver_pos - buff->reconstruct_pos;
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
#ifdef DEBUG_DCBUFFER
		if(current_command_type(buffer)==DC_COPY) {
		    buffer->total_copy_len -= buffer->DCB.full.lb_tail->len;
		}
#endif
		len -= buffer->DCB.full.lb_tail->len;
		DCBufferDecr(buffer);
		buffer->DCB.full.buffer_count--;
	    } else {
#ifdef DEBUG_DCBUFFER
		buffer->total_copy_len -= len;
#endif
		buffer->DCB.full.lb_tail->len -= len;
		len=0;
	    }
	}
	DCBufferIncr(buffer);
    } else if (DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	assert(buffer->DCB.matches.buff_count > 0);
	assert(LLM_VEND(buffer->DCB.matches.cur) - len >= 0);
	trunc_pos = LLM_VEND(buffer->DCB.matches.cur) - len;
	while(buffer->DCB.matches.buff_count > 0 && 
	    trunc_pos < LLM_VEND(buffer->DCB.matches.cur)) {
	    if(buffer->DCB.matches.cur->ver_pos >= trunc_pos) {
#ifdef DEBUG_DCBUFFER
		buffer->total_copy_len -= buffer->DCB.matches.cur->len;
#endif
		DCBufferDecr(buffer);
		buffer->DCB.matches.buff_count--;
	    } else {
#ifdef DEBUG_DCBUFFER
		buffer->total_copy_len -= trunc_pos - 
		    buffer->DCB.matches.cur->ver_pos;
#endif
		buffer->DCB.matches.cur->len = trunc_pos - 
		    buffer->DCB.matches.cur->ver_pos;
	    }
	}
	if(buffer->DCB.matches.buff_count == 0 ) {
	    buffer->reconstruct_pos = buffer->DCB.matches.ver_start;
	} else { 
	    buffer->reconstruct_pos = LLM_VEND(buffer->DCB.matches.cur);
	    DCBufferIncr(buffer);
	}
    } else if(DCBUFFER_LLMATCHES_TYPE == buffer->DCBtype) {
	assert(buffer->DCB.llm.buff_count > 0);
	assert(LLM_VEND(buffer->DCB.llm.cur) - len >= 0);
	trunc_pos = LLM_VEND(buffer->DCB.llm.cur) - len;
	while(buffer->DCB.llm.buff_count > 0 && 
	    trunc_pos < LLM_VEND(buffer->DCB.llm.cur)) {
	    if(buffer->DCB.llm.cur->ver_pos >= trunc_pos) {
#ifdef DEBUG_DCBUFFER
		buffer->total_copy_len -= buffer->DCB.llm.cur->len;
#endif
		buffer->DCB.llm.buff_count--;
		DCBufferDecr(buffer);
	    } else {
#ifdef DEBUG_DCBUFFER
		buffer->total_copy_len -= LLM_VEND(buffer->DCB.llm.cur) - 
		    trunc_pos;
#endif
		buffer->DCB.llm.cur->len = trunc_pos - 
		    buffer->DCB.llm.cur->ver_pos;
	    }
	}
	if(buffer->DCB.llm.buff_count == 0 ) {
	    buffer->reconstruct_pos = buffer->DCB.llm.ver_start;
	} else {
	    buffer->reconstruct_pos = LLM_VEND(buffer->DCB.llm.cur);
	    DCBufferIncr(buffer);
	}
    }
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
    } else if(DCBUFFER_LLMATCHES_TYPE == buffer->DCBtype) {
	if(buffer->flags & DCB_LLM_FINALIZED) {
	    assert(buffer->DCB.llm.main != NULL);
	    buffer->DCB.llm.main = buffer->DCB.llm.main->next;
	} else {
	    assert(buffer->DCB.llm.cur - buffer->DCB.llm.buff < 
		buffer->DCB.llm.buff_size);
	    buffer->DCB.llm.cur++;
	}
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
	} else {
	    buffer->reconstruct_pos = 0;
	}
    } else if (DCBUFFER_LLMATCHES_TYPE == buffer->DCBtype) {
	assert((DCB_LLM_FINALIZED & buffer->flags)==0);
	if(buffer->DCB.llm.cur != buffer->DCB.llm.buff) {
	    assert(buffer->DCB.llm.cur != 0);
	    buffer->DCB.llm.cur--;
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
#ifdef DEBUG_DCBUFFER
    buffer->total_copy_len += len;
#endif

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
    } else if(DCBUFFER_LLMATCHES_TYPE == buffer->DCBtype) {
	assert((DCB_LLM_FINALIZED & buffer->flags)==0);
	if(buffer->DCB.llm.buff_count == buffer->DCB.llm.buff_size) {
	    DCB_resize_llmatches(buffer);
	}
	assert(buffer->DCB.llm.buff_count == (buffer->DCB.llm.cur - 
	   buffer->DCB.llm.buff));
	assert(ver_pos + len <= buffer->ver_size);
	buffer->DCB.llm.cur->src_pos = src_pos;
	buffer->DCB.llm.cur->ver_pos = ver_pos;
	buffer->DCB.llm.cur->len = len;
	buffer->DCB.llm.buff_count++;
	buffer->reconstruct_pos = ver_pos + len;
    }
    DCBufferIncr(buffer);
}

void
DCB_resize_llm_free(CommandBuffer *buff)
{
    if((buff->DCB.llm.free = (void **)realloc(buff->DCB.llm.free, 
	buff->DCB.llm.free_size * 2 * sizeof(void *)))==NULL) {
	abort();
    }
    buff->DCB.llm.free_size *= 2;
}

void
DCB_resize_llmatches(CommandBuffer *buff)
{
    assert(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype);
    assert(buff->DCB.llm.buff_count <= buff->DCB.llm.buff_size);
    v3printf("resizing ll_matches buffer from %lu to %lu\n",
	buff->DCB.llm.buff_size, buff->DCB.llm.buff_size * 2);
    buff->DCB.llm.buff_size *= 2;
    if((buff->DCB.llm.buff = (LL_DCLmatch *)realloc(buff->DCB.llm.buff, 
	buff->DCB.llm.buff_size * sizeof(LL_DCLmatch))) == NULL) {
	v0printf("buffer resize failed\n");
	exit(1);
    }
    buff->DCB.llm.cur = buff->DCB.llm.buff + buff->DCB.llm.buff_count;
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
    } else if(DCBUFFER_LLMATCHES_TYPE == buffer->DCBtype) {
	assert(DCB_LLM_FINALIZED & buffer->flags);
	buffer->DCB.llm.main = buffer->DCB.llm.main_head;
	buffer->DCB.llm.gap_pos = buffer->DCB.llm.ver_start;
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
	} else {
	    x= (buffer->DCB.full.buffer_count - (buffer->DCB.full.lb_tail - 
		buffer->DCB.full.lb_head));
	}
	return x+1 > 1;
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	return (buffer->reconstruct_pos != buffer->ver_size);
    } else if(DCBUFFER_LLMATCHES_TYPE == buffer->DCBtype) {
	assert(DCB_LLM_FINALIZED & buffer->flags);
	return (buffer->reconstruct_pos != buffer->ver_size);
//	return (buffer->DCB.llm.main != NULL);
    }
    return 0;
}
	

void DCBufferFree(CommandBuffer *buffer)
{
    unsigned long x;
    if(buffer->flags & ADD_CFH_FREE_FLAG)
	free(buffer->add_cfh);
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	free(buffer->DCB.full.cb_start);
	free(buffer->DCB.full.lb_start);
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	free(buffer->DCB.matches.buff);
    } else {
	for(x=0; x< buffer->DCB.llm.free_count; x++)
	    free(buffer->DCB.llm.free[x]);
	free(buffer->DCB.llm.free);
    }
}

void 
DCBufferInit(CommandBuffer *buffer, unsigned long buffer_size, 
    unsigned long src_size, unsigned long ver_size, unsigned char type)
{
    buffer->flags =0;
    buffer->src_size = src_size;
    buffer->ver_size = ver_size;
    buffer->reconstruct_pos = 0;
    buffer->DCBtype = type;
#ifdef DEBUG_DCBUFFER
    buffer->total_copy_len = 0;
#endif
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
    } else if(DCBUFFER_LLMATCHES_TYPE == type) {
	buffer->DCB.llm.main = buffer->DCB.llm.main_head = NULL;
	if((buffer->DCB.llm.free = (void **)malloc(10 * sizeof(void *)))
	    ==NULL) {
	    abort();
	}
	buffer->DCB.llm.free_size = 10;
	buffer->DCB.llm.free_count = 0;
	buffer->DCB.llm.buff_count = buffer->DCB.llm.main_count = 
	    buffer->DCB.llm.buff_size = 0;
	buffer->DCB.llm.ver_start = 0;
	buffer->flags |= DCB_LLM_FINALIZED;
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

unsigned int
DCB_test_llm_main(CommandBuffer *buff)
{
    LL_DCLmatch *ptr;
    unsigned long cur_pos, count=0;
    assert(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype);
    if(buff->DCB.llm.main_head != NULL) {
	ptr = buff->DCB.llm.main_head;
	cur_pos = ptr->ver_pos;
	while(ptr != NULL && ptr->next != NULL) {
	    assert(ptr->len < MIN(buff->ver_size, buff->src_size));
	    assert(ptr->ver_pos + ptr->len <= buff->ver_size);
	    assert(ptr->src_pos + ptr->len <= buff->src_size);
	    assert(cur_pos <= ptr->ver_pos + ptr->len);
	    cur_pos = ptr->ver_pos + ptr->len;
	    ptr = ptr->next;
	    count++;
	}
    } else {
	v1printf("main_head is empty, can't test it...\n");
    }
    return 1;
}

int
cmp_llmatch(void *dl1, void *dl2)
{
    LL_DCLmatch *d1 = (LL_DCLmatch *)dl1;
    LL_DCLmatch *d2 = (LL_DCLmatch *)dl2;
    if(d1->ver_pos < d2->ver_pos)
	return -1;
    else if(d1->ver_pos > d2->ver_pos)
	return 1;
    if(d1->len == d2->len) 
	return 0;
    else if(d1->len < d2->len)
	return -1;
    return 1;
}

void
DCB_insert(CommandBuffer *buff)
{
    unsigned long x;
    if(!(buff->DCBtype & DCBUFFER_LLMATCHES_TYPE)) {
	return;
    }
    if(buff->DCB.llm.buff_count > 0 ) {
	buff->DCB.llm.cur--;
	v2printf("inserting a segment %lu:%lu, commands(%lu)\n", 
	    buff->DCB.llm.buff->ver_pos, LLM_VEND(buff->DCB.llm.cur), 
	    buff->DCB.llm.buff_count);

	assert(buff->DCB.llm.main_head==NULL ? buff->DCB.llm.main == NULL : 1);
	if((buff->DCB.llm.buff = (LL_DCLmatch *)realloc(buff->DCB.llm.buff, 
	    buff->DCB.llm.buff_count * sizeof(LL_DCLmatch)))==NULL) {
	    abort();
	}
	// link the buggers
	for(x=0; x < buff->DCB.llm.buff_count -1 ; x++) {
	    buff->DCB.llm.buff[x].next = buff->DCB.llm.buff + x +1;
	}
	buff->DCB.llm.cur = buff->DCB.llm.buff + buff->DCB.llm.buff_count -1;
	if(buff->DCB.llm.main_head == NULL) {
	    //no commands exist yet
	    v2printf("main is empty\n");
	    buff->DCB.llm.main_head = buff->DCB.llm.buff;
	    buff->DCB.llm.main = buff->DCB.llm.cur;
	    buff->DCB.llm.main->next = NULL;
	} else if(buff->DCB.llm.main_head->ver_pos >=
	    buff->DCB.llm.cur->ver_pos) {
	    // prepending it
	    v2printf("prepending commands\n");
	    buff->DCB.llm.cur->next = buff->DCB.llm.main;
	    buff->DCB.llm.main_head = buff->DCB.llm.buff;
	} else {
	    v2printf("gen. insert\n");
	    buff->DCB.llm.cur->next = buff->DCB.llm.main->next;
	    buff->DCB.llm.main->next = buff->DCB.llm.buff;
	    buff->DCB.llm.main = buff->DCB.llm.cur;
	}
	buff->DCB.llm.main_count += buff->DCB.llm.buff_count;
	if(buff->DCB.llm.free_count == buff->DCB.llm.free_size) {
	    DCB_resize_llm_free(buff);
	}
	buff->DCB.llm.free[buff->DCB.llm.free_count++] = buff->DCB.llm.buff;
    } else if(!(buff->flags & DCB_LLM_FINALIZED)){
	free(buff->DCB.llm.buff);
    }
//    assert(buff->DCB.llm.buff->ver_pos >= buff->DCB.llm.main->ver_pos);
    buff->DCB.llm.buff = buff->DCB.llm.cur = NULL;
    buff->DCB.llm.buff_count = buff->DCB.llm.buff_size = 0;
    buff->flags |= DCB_LLM_FINALIZED;
}

void
DCB_llm_init_buff(CommandBuffer *buff, unsigned long buff_size)
{
    assert(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype);
    if((buff->DCB.llm.buff = (LL_DCLmatch *)malloc(buff_size * 
	sizeof(LL_DCLmatch)) )==NULL) {
	perror("shite, malloc failed\n");
	exit(EXIT_FAILURE);
    }
    buff->DCB.llm.cur = buff->DCB.llm.buff;
    buff->DCB.llm.buff_size = buff_size;
    buff->DCB.llm.buff_count = 0;
    buff->flags &= ~DCB_LLM_FINALIZED;
}
