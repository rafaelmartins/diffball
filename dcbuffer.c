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
#include "dcb-cfh-funcs.h"

extern unsigned int verbosity;

int
DCB_register_overlay_srcs(CommandBuffer *dcb, 
    int *id1, cfile *src, dcb_src_read_func rf1, dcb_src_copy_func rc1, 
	char free1,
    int *id2, cfile *add, dcb_src_read_func rf2, dcb_src_copy_func rc2, 
	char free2)
{
    *id1 = internal_DCB_register_cfh_src(dcb, src, 
		rf1, rc1, 
		DC_ADD, (free1 | DCB_OVERLAY_SRC));
    *id2 = internal_DCB_register_cfh_src(dcb, add, 
		rf2, rc2,
		DC_ADD, free2);
    return MIN(*id1, *id2);
}

int
internal_DCB_register_dcb_src(CommandBuffer *dcb, CommandBuffer *dcb_src, 
    dcb_src_read_func read_func, 
    dcb_src_copy_func copy_func,
    unsigned char type, unsigned char flags)
{
    v3printf("registering dcb_src(%x), as buffer id(%u)\n", (unsigned long)dcb, dcb->src_count);
    if(dcb->src_count == dcb->src_array_size && internal_DCB_resize_srcs(dcb)) {
	return MEM_ERROR;
    }
    dcb->srcs[dcb->src_count].dcb = dcb_src;
    dcb->src_flags[dcb->src_count] = flags;
    dcb->src_type[dcb->src_count] = ((type & 0x1) | DCB_DCB_SRC);
    dcb->src_read_func[dcb->src_count] = (read_func == NULL ? default_dcb_src_read_func : read_func);
    dcb->src_copy_func[dcb->src_count] = (copy_func == NULL ? default_dcb_src_copy_func : copy_func);
    return dcb->src_count - 1;
}

int
internal_DCB_register_cfh_src(CommandBuffer *dcb, cfile *cfh, 
    dcb_src_read_func read_func, 
    dcb_src_copy_func copy_func,
    unsigned char type, unsigned char flags)
{
    v3printf("registering cfh_id(%u), as buffer id(%u)\n", cfh->cfh_id, dcb->src_count);
    if(dcb->src_count == dcb->src_array_size && internal_DCB_resize_srcs(dcb)) {
	return MEM_ERROR;
    }
    v2printf("registering %u src\n", dcb->src_count);
    dcb->srcs[dcb->src_count].cfh = cfh;
    dcb->src_flags[dcb->src_count] = flags;
    dcb->src_type[dcb->src_count] = ((type & 0x1) | DCB_CFH_SRC);
    dcb->src_read_func[dcb->src_count] = (read_func == NULL ? default_dcb_src_read_func : read_func);
    dcb->src_copy_func[dcb->src_count] = (copy_func == NULL ? default_dcb_src_copy_func : copy_func);
    dcb->src_count++;
    return dcb->src_count - 1;
}

int
internal_DCB_resize_srcs(CommandBuffer *dcb)
{
    if((dcb->srcs = (dcb_src *)realloc(dcb->srcs, sizeof(dcb_src) 
	* dcb->src_array_size * 2))==NULL) {
	return MEM_ERROR;
    } else if((dcb->src_read_func = (dcb_src_read_func *)realloc(dcb->src_read_func, 
	sizeof(dcb_src_read_func) * dcb->src_array_size * 2))==NULL) {
	return MEM_ERROR;
    } else if((dcb->src_copy_func = (dcb_src_copy_func *)realloc(dcb->src_copy_func, 
	sizeof(dcb_src_copy_func) * dcb->src_array_size * 2))==NULL) {
	return MEM_ERROR;
    } else if((dcb->extra_offsets = (DCLoc **)realloc(dcb->extra_offsets,
	sizeof(DCLoc *) * dcb->src_array_size * 2))==NULL) {
	return MEM_ERROR;
    }
    dcb->src_array_size *= 2;
    return 0;
}

unsigned long 
inline current_command_type(CommandBuffer *buff)
{
    unsigned long x;
    if(DCBUFFER_FULL_TYPE == buff->DCBtype) {
//        return ((*buff->DCB.full.cb_tail >> buff->DCB.full.cb_tail_bit) & 0x1);
	x = buff->DCB.full.src_id[buff->DCB.full.command_pos];
	return (buff->src_type[x] & 0x1);
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
            computed_len += dc.data.len;
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
    while(buff->DCB.llm.main != NULL  && 
	buff->DCB.llm.gap_pos > buff->DCB.llm.main->ver_pos) {
	    DCBufferIncr(buff);
    }
    while(buff->DCB.llm.main != NULL && dc->len == 0) {
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
    if(dc->len != 0) {
	return 1;
    }
    return 0;
}

void
DCB_get_next_command(CommandBuffer *buff, DCommand *dc)
{
    do {
	internal_DCB_get_next_command(buff, dc);
    } while (buff->src_flags[dc->src_id] & DCB_OVERLAY_SRC);
}


void
internal_DCB_get_next_command(CommandBuffer *buff, DCommand *dc)
{
    if(DCBUFFER_FULL_TYPE == buff->DCBtype) {
	dc->type = current_command_type(buff);
	dc->src_id = buff->DCB.full.src_id[buff->DCB.full.command_pos];
	dc->data.src_pos = buff->DCB.full.lb_tail->offset;
	dc->data.ver_pos = buff->reconstruct_pos;
	dc->data.len = buff->DCB.full.lb_tail->len;
	dc->cmd_pos = buff->DCB.full.command_pos;
	DCBufferIncr(buff);
    } else if (DCBUFFER_MATCHES_TYPE == buff->DCBtype) {
	dc->src_id = 0;
	assert(buff->reconstruct_pos != buff->ver_size);
	dc->cmd_pos = 0;
	if((buff->DCB.matches.cur - buff->DCB.matches.buff) == 
	    buff->DCB.matches.buff_count) {
	    dc->type = DC_ADD;
	    dc->data.src_pos = dc->data.ver_pos = buff->reconstruct_pos;
	    dc->data.len = buff->ver_size - buff->reconstruct_pos;
	} else if(buff->reconstruct_pos == buff->DCB.matches.cur->ver_pos) {
	    dc->type = DC_COPY;
	    dc->data.src_pos = buff->DCB.matches.cur->src_pos;
	    dc->data.ver_pos = buff->DCB.matches.cur->ver_pos;
	    dc->data.len = buff->DCB.matches.cur->len;
	    DCBufferIncr(buff);
	} else {
	    dc->type = DC_ADD;
	    dc->data.src_pos = dc->data.ver_pos = buff->reconstruct_pos;
	    dc->data.len = buff->DCB.matches.cur->ver_pos - buff->reconstruct_pos;
	}
    } else if(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype) {
	dc->src_id = 0;
	assert(buff->flags & DCB_LLM_FINALIZED);
	dc->cmd_pos = 0;
	if(buff->DCB.llm.main == NULL) {
	    dc->type = DC_ADD;
	    dc->data.src_pos = dc->data.ver_pos  = buff->reconstruct_pos;
	    dc->data.len = buff->ver_size - buff->reconstruct_pos;
	} else if(buff->reconstruct_pos == buff->DCB.llm.main->ver_pos) {
	    dc->type = DC_COPY;
	    dc->data.src_pos = buff->DCB.llm.main->src_pos;
	    dc->data.ver_pos = buff->reconstruct_pos;
	    dc->data.len = buff->DCB.llm.main->len;
	    DCBufferIncr(buff);
	} else {
	    dc->type = DC_ADD;
	    dc->data.src_pos = dc->data.ver_pos = buff->reconstruct_pos;
	    dc->data.len = buff->DCB.llm.main->ver_pos - buff->reconstruct_pos;
	}
    }
    dc->src_dcb = buff;
    buff->reconstruct_pos += dc->data.len;

}


void 
DCB_truncate(CommandBuffer *buffer, unsigned long len)
{
    unsigned long trunc_pos;
    /* get the tail to an actual node. */
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
	buffer->DCB.full.command_pos++;
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
	buffer->DCB.full.command_pos--;
/*	if (buffer->DCB.full.cb_tail_bit != 0) {
	    buffer->DCB.full.cb_tail_bit--;
	} else {
	    buffer->DCB.full.cb_tail = (buffer->DCB.full.cb_tail == 
		buffer->DCB.full.cb_start) ? buffer->DCB.full.cb_end : 
	        buffer->DCB.full.cb_tail - 1;
	    buffer->DCB.full.cb_tail_bit=7;
	}*/
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
DCB_add_overlay(CommandBuffer *buffer, off_u32 src_pos, int diff_src_id,
    unsigned long add_pos, unsigned long len, int add_id)
{
    DCB_add_add(buffer, src_pos, 0, diff_src_id);
    DCB_add_add(buffer, add_pos, len, add_id);
}

void 
DCB_add_add(CommandBuffer *buffer, unsigned long ver_pos, 
    unsigned long len, unsigned char src_id)
{
#ifdef DEV_VERSION
    v3printf("add v(%lu), l(%lu), id(%u), rpos(%lu)\n", ver_pos , len, src_id, 
	buffer->reconstruct_pos);
#endif
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	if(buffer->DCB.full.lb_tail == buffer->DCB.full.lb_end)
	    internal_DCB_resize_full(buffer);

	buffer->DCB.full.src_id[buffer->DCB.full.command_pos] = src_id;
	buffer->DCB.full.lb_tail->offset = ver_pos;
	buffer->DCB.full.lb_tail->len = len;
	buffer->DCB.full.buffer_count++;
	buffer->reconstruct_pos += len;
	DCBufferIncr(buffer);
    }
}

void
DCB_add_copy(CommandBuffer *buffer, unsigned long src_pos, 
    unsigned long ver_pos, unsigned long len, unsigned char src_id)
{
#ifdef DEBUG_DCBUFFER
    buffer->total_copy_len += len;
#endif
#ifdef DEV_VERSION
    v3printf("copy s(%lu), v(%lu), l(%lu), rpos(%lu)\n", src_pos, ver_pos ,
	 len, buffer->reconstruct_pos);
#endif

    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	if(buffer->DCB.full.lb_tail == buffer->DCB.full.lb_end)
	    internal_DCB_resize_full(buffer);
	buffer->DCB.full.lb_tail->offset = src_pos;
	buffer->DCB.full.lb_tail->len = len;
//	*buffer->DCB.full.cb_tail |= (1 << buffer->DCB.full.cb_tail_bit);
	buffer->DCB.full.src_id[buffer->DCB.full.command_pos] = src_id;
	buffer->DCB.full.buffer_count++;
	buffer->reconstruct_pos += len;
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	if(buffer->DCB.matches.buff_count == buffer->DCB.matches.buff_size) {
	    internal_DCB_resize_matches(buffer);
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
	    internal_DCB_resize_llmatches(buffer);
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

int
internal_DCB_resize_llm_free(CommandBuffer *buff)
{
    if((buff->DCB.llm.free = (void **)realloc(buff->DCB.llm.free, 
	buff->DCB.llm.free_size * 2 * sizeof(void *)))==NULL) {
	return MEM_ERROR;
    }
    buff->DCB.llm.free_size *= 2;
    return 0;
}

int
internal_DCB_resize_llmatches(CommandBuffer *buff)
{
    assert(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype);
    assert(buff->DCB.llm.buff_count <= buff->DCB.llm.buff_size);
    v3printf("resizing ll_matches buffer from %lu to %lu\n",
	buff->DCB.llm.buff_size, buff->DCB.llm.buff_size * 2);
    buff->DCB.llm.buff_size *= 2;
    if((buff->DCB.llm.buff = (LL_DCLmatch *)realloc(buff->DCB.llm.buff, 
	buff->DCB.llm.buff_size * sizeof(LL_DCLmatch))) == NULL) {
	return MEM_ERROR;
    }
    buff->DCB.llm.cur = buff->DCB.llm.buff + buff->DCB.llm.buff_count;
    return 0;
}

void 
DCBufferCollapseAdds(CommandBuffer *buffer)
{
/*    unsigned long count, *plen;
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
    }*/
}



void
DCBufferReset(CommandBuffer *buffer)
{
    buffer->reconstruct_pos = 0;
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	buffer->DCB.full.lb_tail = buffer->DCB.full.lb_start;
	buffer->DCB.full.command_pos = 0;
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
	

void 
DCBufferFree(CommandBuffer *buffer)
{
    unsigned long x;
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	free(buffer->DCB.full.lb_start);
	buffer->DCB.full.lb_start = NULL;
	free(buffer->DCB.full.src_id);
	buffer->DCB.full.src_id = NULL;
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	free(buffer->DCB.matches.buff);
	buffer->DCB.matches.buff = NULL;
    } else {
	for(x=0; x< buffer->DCB.llm.free_count; x++)
	    free(buffer->DCB.llm.free[x]);
	free(buffer->DCB.llm.free);
	buffer->DCB.llm.free = NULL;
    }
    for(x=0; x < buffer->src_count; x++) {
	if(buffer->src_flags[x] & DCB_FREE_SRC_CFH) {
	    v2printf("cclosing src_cfh(%lu)\n", x);
	    cclose(buffer->srcs[x].cfh);
	    v2printf("freeing  src_cfh(%lu)\n", x);
	    free(buffer->srcs[x].cfh);
	}
    }
    for(x=0; x < 256; x++)
	buffer->src_flags[x] = 0;
    for(x=0; x < 16; x++)
	buffer->src_type[x] = 0;

    free(buffer->src_read_func);
    free(buffer->src_copy_func);
    free(buffer->srcs);
    buffer->src_count = buffer->src_array_size = 0;
}

int 
DCBufferInit(CommandBuffer *buffer, unsigned long buffer_size, 
    unsigned long src_size, unsigned long ver_size, unsigned char type)
{
    buffer->flags =0;
    buffer->src_size = src_size;
    buffer->ver_size = ver_size;
    buffer->reconstruct_pos = 0;
    buffer->DCBtype = type;
    memset(buffer->src_flags, 0, 256);
    memset(buffer->src_type, 0, 16);
#ifdef DEBUG_DCBUFFER
    buffer->total_copy_len = 0;
#endif
    buffer->src_array_size = 4;
    if((buffer->srcs = (dcb_src *)malloc(sizeof(dcb_src) * buffer->src_array_size))==NULL) {
	return MEM_ERROR;
    } else if((buffer->extra_offsets = (DCLoc **)malloc(sizeof(DCLoc *) * buffer->src_array_size))==NULL) {
	return MEM_ERROR;
    } else if ((buffer->src_read_func = (dcb_src_read_func *)malloc(sizeof(dcb_src_read_func) * buffer->src_array_size))==NULL) {
	return MEM_ERROR;
    } else if ((buffer->src_copy_func = (dcb_src_copy_func *)malloc(sizeof(dcb_src_copy_func) * buffer->src_array_size))==NULL) {
	return MEM_ERROR;
    }
    buffer->src_count = 0;
    if(type == DCBUFFER_FULL_TYPE) {
	buffer->DCB.full.buffer_count = 0;
	buffer->DCB.full.buffer_size = buffer_size;

	if((buffer->DCB.full.src_id = (unsigned char *)malloc(buffer_size))
	    == NULL){
	    return MEM_ERROR;
	} else if((buffer->DCB.full.lb_start = (DCLoc *)malloc(sizeof(DCLoc) *
	    buffer_size)) == NULL) {
	    return MEM_ERROR;
	}

/*	buffer->DCB.full.cb_head = buffer->DCB.full.cb_tail = 
	    buffer->DCB.full.cb_start;
	buffer->DCB.full.cb_end = buffer->DCB.full.cb_start + buffer_size - 1;
	buffer->DCB.full.cb_head_bit = buffer->DCB.full.cb_tail_bit = 0;
*/
        buffer->DCB.full.lb_head = buffer->DCB.full.lb_tail = 
	    buffer->DCB.full.lb_start;
	buffer->DCB.full.lb_end = buffer->DCB.full.lb_start + 
	    buffer->DCB.full.buffer_size - 1;
//	buffer->DCB.full.add_index=0;
	buffer->DCB.full.command_pos = 0;
    } else if(DCBUFFER_MATCHES_TYPE == type) {
	if((buffer->DCB.matches.buff = (DCLoc_match *)malloc(buffer_size * 
	    sizeof(DCLoc_match)) )==NULL) {
	    return MEM_ERROR;
	}
	buffer->DCB.matches.cur = buffer->DCB.matches.buff;
	buffer->DCB.matches.buff_size = buffer_size;
	buffer->DCB.matches.buff_count = 0;
    } else if(DCBUFFER_LLMATCHES_TYPE == type) {
	buffer->DCB.llm.main = buffer->DCB.llm.main_head = NULL;
	if((buffer->DCB.llm.free = (void **)malloc(10 * sizeof(void *)))
	    ==NULL) {
	    return MEM_ERROR;
	}
	buffer->DCB.llm.free_size = 10;
	buffer->DCB.llm.free_count = 0;
	buffer->DCB.llm.buff_count = buffer->DCB.llm.main_count = 
	    buffer->DCB.llm.buff_size = 0;
	buffer->DCB.llm.ver_start = 0;
	buffer->flags |= DCB_LLM_FINALIZED;
    }
    return 0;
}

int
internal_DCB_resize_matches(CommandBuffer *buffer)
{
    assert(DCBUFFER_MATCHES_TYPE == buffer->DCBtype);
    v1printf("resizing matches buffer from %lu to %lu\n", 
	buffer->DCB.matches.buff_size, buffer->DCB.matches.buff_size * 2);
    buffer->DCB.matches.buff_size *= 2;
    if((buffer->DCB.matches.buff = (DCLoc_match *)realloc(
	buffer->DCB.matches.buff, buffer->DCB.matches.buff_size * 
	sizeof(DCLoc_match))) == NULL) {
	return MEM_ERROR;
    }
    buffer->DCB.matches.cur = buffer->DCB.matches.buff + 
	buffer->DCB.matches.buff_count;
    return 0;
}

int
internal_DCB_resize_full(CommandBuffer *buffer)
{
    assert(DCBUFFER_FULL_TYPE == buffer->DCBtype);
    v1printf("resizing command buffer from %lu to %lu\n", 
	buffer->DCB.full.buffer_size, buffer->DCB.full.buffer_size * 2);

    if((buffer->DCB.full.src_id = (unsigned char *)realloc(
	buffer->DCB.full.src_id, buffer->DCB.full.buffer_size *2))
	==NULL) {
	return MEM_ERROR;
    } else if((buffer->DCB.full.lb_start = 
	(DCLoc *)realloc(buffer->DCB.full.lb_start, 
	buffer->DCB.full.buffer_size * 2 * sizeof(DCLoc)) )==NULL) {
	return MEM_ERROR;
    }
    buffer->DCB.full.buffer_size *= 2;
    buffer->DCB.full.lb_head = buffer->DCB.full.lb_start;
    buffer->DCB.full.lb_tail = buffer->DCB.full.lb_start + 
	buffer->DCB.full.buffer_count;
    buffer->DCB.full.lb_end = buffer->DCB.full.lb_start + 
	buffer->DCB.full.buffer_size -1;

/*    buffer->DCB.full.cb_head = buffer->DCB.full.cb_start;
    buffer->DCB.full.cb_tail = buffer->DCB.full.cb_start + 
	(buffer->DCB.full.buffer_count/8);
    buffer->DCB.full.cb_end = buffer->DCB.full.cb_start + 
	(buffer->DCB.full.buffer_size/8) -1;*/
    return 0;
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

int
DCB_insert(CommandBuffer *buff)
{
    unsigned long x;
    if(!(buff->DCBtype & DCBUFFER_LLMATCHES_TYPE)) {
	return 0;
    }
    if(buff->DCB.llm.buff_count > 0 ) {
	buff->DCB.llm.cur--;
	v2printf("inserting a segment %lu:%lu, commands(%lu)\n", 
	    buff->DCB.llm.buff->ver_pos, LLM_VEND(buff->DCB.llm.cur), 
	    buff->DCB.llm.buff_count);

	assert(buff->DCB.llm.main_head==NULL ? buff->DCB.llm.main == NULL : 1);
	if((buff->DCB.llm.buff = (LL_DCLmatch *)realloc(buff->DCB.llm.buff, 
	    buff->DCB.llm.buff_count * sizeof(LL_DCLmatch)))==NULL) {
	    return MEM_ERROR;
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
	    internal_DCB_resize_llm_free(buff);
	}
	buff->DCB.llm.free[buff->DCB.llm.free_count++] = buff->DCB.llm.buff;
    } else if(!(buff->flags & DCB_LLM_FINALIZED)){
	free(buff->DCB.llm.buff);
    }
//    assert(buff->DCB.llm.buff->ver_pos >= buff->DCB.llm.main->ver_pos);
    buff->DCB.llm.buff = buff->DCB.llm.cur = NULL;
    buff->DCB.llm.buff_count = buff->DCB.llm.buff_size = 0;
    buff->flags |= DCB_LLM_FINALIZED;
    return 0;
}

int
DCB_llm_init_buff(CommandBuffer *buff, unsigned long buff_size)
{
    assert(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype);
    if((buff->DCB.llm.buff = (LL_DCLmatch *)malloc(buff_size * 
	sizeof(LL_DCLmatch)) )==NULL) {
	return MEM_ERROR;
    }
    buff->DCB.llm.cur = buff->DCB.llm.buff;
    buff->DCB.llm.buff_size = buff_size;
    buff->DCB.llm.buff_count = 0;
    buff->flags &= ~DCB_LLM_FINALIZED;
    return 0;
}

