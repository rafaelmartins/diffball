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
#include <math.h>

#include "dcbuffer.h"
#include "cfile.h"
#include "bit-functions.h"
#include "defs.h"
#include "dcb-cfh-funcs.h"

extern unsigned int verbosity;

unsigned long
bail_if_called_func()
{
    v0printf("err, wtf\n");
    abort();
}
        

int
DCB_collapse_commands(CommandBuffer *dcb, DCommand_abbrev **dptr_p, 
    unsigned long *len1, DCommand_abbrev **odptr_p, unsigned long *len2)
{
//  ahem. bit hackney.  needs work.
    unsigned long 	x, y, count;
    off_u64		ver_pos;
    DCommand_abbrev *dptr, *odptr, *base;

    DCBufferReset(dcb);
    count=0;
    for(x=0; x < dcb->DCB.full.cl.com_count; x++) {
    	if(dcb->srcs[dcb->DCB.full.cl.src_id[x]].ov.com_count == 0) {
    	    count++;
    	}
    }
    for(x=0; x < dcb->src_count; x++) {
	for(y = 0; y < dcb->srcs[x].ov.com_count; y++) {

	    // don't add in the ov DCLoc pointing at another chain, only the mask and base commands.
	    if(dcb->srcs[dcb->srcs[x].ov.src_id[y]].ov.com_count == 0 || dcb->srcs[x].ov.src_id[y] == x)
	    	count++;
	}
    }
    printf("count=%lu\n", count);
    if((base = (DCommand_abbrev *)malloc(sizeof(DCommand_abbrev) * count)) 
    	== NULL) {
    	return MEM_ERROR;
    }
    dptr = base;
    *len1 = 0;
    *len2 = 0;
    ver_pos = 0;
    odptr = base + count -1;
    while(odptr >= dptr) {
    	// check if it's an overlay.
	if(dcb->srcs[dcb->DCB.full.cl.src_id[dcb->DCB.full.command_pos]].ov.com_count) {
	    ver_pos = process_ovchain(dcb, ver_pos, &dptr, &odptr, 
		&dcb->srcs[dcb->DCB.full.cl.src_id[dcb->DCB.full.command_pos]].ov,
		dcb->DCB.full.cl.command[dcb->DCB.full.command_pos].offset,
		dcb->DCB.full.cl.command[dcb->DCB.full.command_pos].len);
	} else {
    	    dptr->data.src_pos = dcb->DCB.full.cl.command[dcb->DCB.full.command_pos].offset;
            dptr->data.len = dcb->DCB.full.cl.command[dcb->DCB.full.command_pos].len;
	    dptr->src_id = dcb->DCB.full.cl.src_id[dcb->DCB.full.command_pos];
            dptr->data.ver_pos = ver_pos;
	    ver_pos += dptr->data.len;
	    dptr++;
        }
//	ver_pos += dcb->DCB.full.lb_tail->len;
        DCBufferIncr(dcb);
    }
    dptr = base;
    if(odptr < base || dcb->srcs[odptr->src_id].ov.com_count == 0) {
	// this means either everything was overlays, or last DCLoc processed
	// was overlay
	odptr++;
    }
    *len1 = odptr - base;
    *len2 = count - (odptr - base);
    *odptr_p = odptr;
    *dptr_p = dptr;
    DCBufferReset(dcb);
    return 0;
}

off_u64
process_ovchain(CommandBuffer *dcb, off_u64 ver_pos, 
    DCommand_abbrev **dptr, DCommand_abbrev **odptr, overlay_chain *ov,
    unsigned long offset, unsigned long len)
{
    //first command is *always* the mask command.
    (*odptr)->data.ver_pos = ver_pos;
    (*odptr)->data.len = ov->command[offset].len;
    (*odptr)->data.src_pos = ov->command[offset].offset;
    (*odptr)->src_id = ov->src_id[offset];
    *odptr -= 1;
    len--;
    offset++;
    while(len) {
    	if(dcb->srcs[ov->src_id[offset]].ov.com_count) {
	    // tiz an overlay command.
	    ver_pos = process_ovchain(dcb, ver_pos, dptr, odptr,
	    	&dcb->srcs[ov->src_id[offset]].ov, 
	    	ov->command[offset].offset,
	    	ov->command[offset].len);
	} else {
	    (*dptr)->data.ver_pos = ver_pos;
	    (*dptr)->data.len = ov->command[offset].len;
	    (*dptr)->data.src_pos = ov->command[offset].offset;
	    (*dptr)->src_id = ov->src_id[offset];
	    ver_pos += ov->command[offset].len;
	    *dptr += 1;
	}
	len--;
	offset++;
    }
    return ver_pos;
}

int
DCB_register_overlay_src(CommandBuffer *dcb, 
    cfile *src, dcb_src_read_func rf1, dcb_src_copy_func rc1, 
    dcb_src_read_func rm1, char free1)
{
    int id;
    id = internal_DCB_register_cfh_src(dcb, src, 
		rf1, rc1, 
		DC_ADD, (free1 | DCB_OVERLAY_SRC));
    if(id < 0) {
    	return id;
    }
    if(rm1 != NULL)
	dcb->srcs[id].mask_read_func = rm1;
    else
    	dcb->srcs[id].mask_read_func = default_dcb_src_cfh_read_func;

    dcb->srcs[id].ov.com_size = 4;
    dcb->srcs[id].ov.com_count = 0;
    if((dcb->srcs[id].ov.command = (DCLoc *)malloc(sizeof(DCLoc) *
    	dcb->srcs[id].ov.com_size))==NULL) {
    	return MEM_ERROR;
    } else if((dcb->srcs[id].ov.src_id = (unsigned char *)malloc(
    	sizeof(unsigned char) * dcb->srcs[id].ov.com_size))==NULL) {
    	return MEM_ERROR;
    }
    return id;
}

int
DCB_register_dcb_src(CommandBuffer *dcb, CommandBuffer *dcb_src)
{
    unsigned short x;
    assert(dcb->DCBtype == DCBUFFER_FULL_TYPE);
    assert(dcb_src->DCBtype == DCBUFFER_FULL_TYPE);
    if((dcb->srcs[dcb->src_count].src_ptr.dcb = (DCB_src *)malloc(
    	sizeof(DCB_src))) == NULL) {
    	return MEM_ERROR;
    }
    for(x=0; x < 256; x++)
    	dcb->srcs[dcb->src_count].src_ptr.dcb->src_map[x] = DCB_SRC_NOT_TRANSLATED;
    	
    dcb->srcs[dcb->src_count].src_ptr.dcb->src_dcb = dcb_src;
    dcb->srcs[dcb->src_count].src_ptr.dcb->s = create_DCBSearch_index(dcb_src);
    if(dcb->srcs[dcb->src_count].src_ptr.dcb->s == NULL) {
    	free(dcb->srcs[dcb->src_count].src_ptr.dcb);
    	return MEM_ERROR;
    }
    dcb->srcs[dcb->src_count].flags = 0;
    dcb->srcs[dcb->src_count].read_func = NULL;
    dcb->srcs[dcb->src_count].mask_read_func = NULL;
    dcb->srcs[dcb->src_count].copy_func = NULL;
    dcb->srcs[dcb->src_count].ov.src_id = NULL;
    dcb->srcs[dcb->src_count].ov.command = NULL;
    dcb->srcs[dcb->src_count].ov.com_size = 0;
    dcb->srcs[dcb->src_count].ov.com_count = 0;
    dcb->srcs[dcb->src_count].type = ((DC_COPY & 0x1) | DCB_DCB_SRC);
    dcb->src_count++;
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
    dcb->srcs[dcb->src_count].src_ptr.cfh = cfh;
    dcb->srcs[dcb->src_count].flags = flags;
    dcb->srcs[dcb->src_count].type = ((type & 0x1) | DCB_CFH_SRC);
    dcb->srcs[dcb->src_count].read_func = (read_func == NULL ? default_dcb_src_cfh_read_func : read_func);
    dcb->srcs[dcb->src_count].copy_func = (copy_func == NULL ? default_dcb_src_cfh_copy_func : copy_func);
    dcb->srcs[dcb->src_count].mask_read_func = NULL;
    dcb->srcs[dcb->src_count].ov.src_id = NULL;
    dcb->srcs[dcb->src_count].ov.command = NULL;
    dcb->srcs[dcb->src_count].ov.com_size = 0;
    dcb->srcs[dcb->src_count].ov.com_count = 0;
    if(DCBUFFER_FULL_TYPE != dcb->DCBtype) {
	if(type & DC_COPY) {
	    dcb->default_copy_src = dcb->srcs + dcb->src_count;
	} else {
	    dcb->default_add_src = dcb->srcs + dcb->src_count;
	}
    }
    dcb->src_count++;
    return dcb->src_count - 1;
}

int
internal_DCB_resize_srcs(CommandBuffer *dcb)
{
    if((dcb->srcs = (DCB_registered_src *)realloc(dcb->srcs, sizeof(DCB_registered_src) 
	* dcb->src_array_size * 2))==NULL) {
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
	x = buff->DCB.full.cl.src_id[buff->DCB.full.command_pos];
	return (buff->srcs[x].type & 0x1);
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
internal_DCB_get_next_command(CommandBuffer *buff, DCommand *dc)
{
    if(DCBUFFER_FULL_TYPE == buff->DCBtype) {
	dc->type = current_command_type(buff);
//	dc->data.src_pos = buff->DCB.full.lb_tail->offset;
	dc->data.ver_pos = buff->reconstruct_pos;
//	dc->data.len = buff->DCB.full.lb_tail->len;
	dc->dcb_src = buff->srcs + buff->DCB.full.cl.src_id[buff->DCB.full.command_pos];
	if(dc->dcb_src->ov.com_count) {
	    dc->ov_offset = buff->DCB.full.cl.command[buff->DCB.full.command_pos].offset;
	    dc->ov_len = buff->DCB.full.cl.command[buff->DCB.full.command_pos].len;
	    dc->data.src_pos = dc->dcb_src->ov.command[dc->ov_offset].offset;
	    dc->data.len = dc->dcb_src->ov.command[dc->ov_offset].len;
	} else {
	    dc->data.src_pos = buff->DCB.full.cl.command[buff->DCB.full.command_pos].offset;
	    dc->data.len = buff->DCB.full.cl.command[buff->DCB.full.command_pos].len;
	    dc->ov_offset = 0;
	    dc->ov_len = 0;
	}
	DCBufferIncr(buff);
    } else if (DCBUFFER_MATCHES_TYPE == buff->DCBtype) {
	assert(buff->reconstruct_pos != buff->ver_size);
	if((buff->DCB.matches.cur - buff->DCB.matches.buff) == 
	    buff->DCB.matches.buff_count) {
	    dc->type = DC_ADD;
	    dc->dcb_src = buff->default_add_src;
	    dc->data.src_pos = dc->data.ver_pos = buff->reconstruct_pos;
	    dc->data.len = buff->ver_size - buff->reconstruct_pos;
	} else if(buff->reconstruct_pos == buff->DCB.matches.cur->ver_pos) {
	    dc->type = DC_COPY;
	    dc->dcb_src = buff->default_copy_src;
	    dc->data.src_pos = buff->DCB.matches.cur->src_pos;
	    dc->data.ver_pos = buff->DCB.matches.cur->ver_pos;
	    dc->data.len = buff->DCB.matches.cur->len;
	    DCBufferIncr(buff);
	} else {
	    dc->type = DC_ADD;
	    dc->dcb_src = buff->default_add_src;
	    dc->data.src_pos = dc->data.ver_pos = buff->reconstruct_pos;
	    dc->data.len = buff->DCB.matches.cur->ver_pos - buff->reconstruct_pos;
	}
	dc->ov_offset = 0;
	dc->ov_len = 0;
    } else if(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype) {
	assert(buff->flags & DCB_LLM_FINALIZED);
	if(buff->DCB.llm.main == NULL) {
	    dc->type = DC_ADD;
	    dc->dcb_src = buff->default_add_src;
	    dc->data.src_pos = buff->reconstruct_pos;
	    dc->data.ver_pos = buff->reconstruct_pos;
	    dc->data.len = buff->ver_size - buff->reconstruct_pos;
	} else if(buff->reconstruct_pos == buff->DCB.llm.main->ver_pos) {
	    dc->type = DC_COPY;
	    dc->dcb_src = buff->default_copy_src;
	    dc->data.src_pos = buff->DCB.llm.main->src_pos;
	    dc->data.ver_pos = buff->reconstruct_pos;
	    dc->data.len = buff->DCB.llm.main->len;
	    DCBufferIncr(buff);
	} else {
	    dc->type = DC_ADD;
	    dc->dcb_src = buff->default_add_src;
	    dc->data.src_pos = buff->reconstruct_pos;
	    dc->data.ver_pos = buff->reconstruct_pos;
	    dc->data.len = buff->DCB.llm.main->ver_pos - buff->reconstruct_pos;
	}
	dc->ov_offset = 0;
	dc->ov_len = 0;
    }
    dc->dcb_ptr = buff;
    dc->src_id = dc->dcb_src - buff->srcs;
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
	    if (buffer->DCB.full.cl.command[buffer->DCB.full.command_pos].len <= len) {

#ifdef DEBUG_DCBUFFER
		if(current_command_type(buffer)==DC_COPY) {
		    buffer->total_copy_len -= buffer->DCB.full.cl.command[buffer->DCB.full.command_pos].len;
		}
#endif

		len -= buffer->DCB.full.cl.command[buffer->DCB.full.command_pos].len;
		DCBufferDecr(buffer);
		buffer->DCB.full.cl.com_count--;
	    } else {
#ifdef DEBUG_DCBUFFER
		buffer->total_copy_len -= len;
#endif
		buffer->DCB.full.cl.command[buffer->DCB.full.command_pos].len -= len;
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
	buffer->DCB.full.command_pos++;
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
	buffer->DCB.full.command_pos--;
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

/* at the moment, this is designed to basically give the finger when it detects a DCB_SRC_DCB, w/in sdcb.
   I don't want to get into recursively registering DCB's as srcs through versions- this may (likely will)
   change down the line, once this code has been stabled, and I feel the need/urge.

   meanwhile, suffer the assert :)
*/
int
DCB_rec_copy_from_DCB_src(CommandBuffer *tdcb, command_list *tcl,
    CommandBuffer *sdcb, command_list *scl, unsigned short translation_map[256],
    unsigned long com_offset, off_u64 seek, off_u64 len)
{
    unsigned long index;
    off_u64 tmp_len;
    signed short int x;
    DCLoc *cur;
    DCB_registered_src *dcb_s;

    assert(sdcb == tdcb || translation_map != NULL);
    assert(com_offset < scl->com_count);

    // adjust position appropriately
    if(sdcb->srcs[scl->src_id[com_offset]].ov.com_size && &sdcb->srcs[scl->src_id[com_offset]].ov != scl)
    	tmp_len = sdcb->srcs[scl->src_id[com_offset]].ov.command[scl->command[com_offset].offset].len;
    else
    	tmp_len = scl->command[com_offset].len;
    while(seek >= tmp_len) {
	seek -= tmp_len;
    	com_offset++;
	assert(com_offset < scl->com_count);

    	if(seek) {
    	
    	    if(sdcb->srcs[scl->src_id[com_offset]].ov.com_size)
    	    	tmp_len = sdcb->srcs[scl->src_id[com_offset]].ov.command[scl->command[com_offset].offset].len;
    	    else
    	    	tmp_len = scl->command[com_offset].len;
    	}
    	   
    }

    while(len) {
	cur = scl->command + com_offset;
	dcb_s = sdcb->srcs + scl->src_id[com_offset];

	tmp_len = MIN(cur->len - seek, len);

    	if(dcb_s->type & DCB_DCB_SRC) {

	    /* only allow translating one dcb version; other wise would have to recursively 
	       update maps to tdcb */

	    index = cur->offset / dcb_s->src_ptr.dcb->s->quanta;

	    assert(sdcb == tdcb);
	    assert(index < dcb_s->src_ptr.dcb->s->index_size);
	    assert(dcb_s->src_ptr.dcb->s->ver_start[index] <= cur->offset);

	    if(DCB_rec_copy_from_DCB_src(tdcb, tcl, 
	    	dcb_s->src_ptr.dcb->src_dcb,
	    	&dcb_s->src_ptr.dcb->src_dcb->DCB.full.cl,
	    	dcb_s->src_ptr.dcb->src_map,
		dcb_s->src_ptr.dcb->s->index[index],
		cur->offset - dcb_s->src_ptr.dcb->s->ver_start[index] + seek,
		tmp_len)) {
	    }
	} else {
	    if(tcl->com_count == tcl->com_size) {
	    	if(internal_DCB_resize_cl(tcl))
	    	    return MEM_ERROR;
	    }
	    
	    if(sdcb != tdcb) {

	    	/* we're not working on the same version, so the map must be used, and updated. */
	    	if(translation_map[scl->src_id[com_offset]] == DCB_SRC_NOT_TRANSLATED) {

		    if(dcb_s->ov.com_count == 0) {
			x = DCB_register_src(tdcb, dcb_s->src_ptr.cfh, 
	    	    	    dcb_s->read_func, dcb_s->copy_func, 
	    	    	    dcb_s->flags, (dcb_s->type & 0x1));
	    	    } else {
	    	    	x = DCB_register_overlay_src(tdcb, dcb_s->src_ptr.cfh,
	    	    	    dcb_s->read_func, dcb_s->copy_func, dcb_s->mask_read_func,
	    	    	    (dcb_s->flags & DCB_FREE_SRC_CFH));
	    	    }
	    	    if(x < 0)
	    		return x;

		    translation_map[scl->src_id[com_offset]] = x;
		    /* disable auto-freeing in the parent; leave the flag on tdcb's version of src */
	    	    dcb_s->flags &= ~DCB_FREE_SRC_CFH;
	    	} else {
		    x = translation_map[scl->src_id[com_offset]];
	    	}
	    } else {
	    	x = scl->src_id[com_offset];
	    }
	    if(sdcb->srcs[scl->src_id[com_offset]].ov.com_count && scl != &sdcb->srcs[scl->src_id[com_offset]].ov) {
	    	/* this is an ov, and we're not processing the first masking command.
	    	   so, we go recursive 
	    	   note temp_len is inaccurate when encountering the command to jump from ov_chain 
	    	   to another */

	    	tmp_len = MIN(len, sdcb->srcs[scl->src_id[com_offset]].ov.command[cur->offset].len - seek);
	    	index = tdcb->srcs[x].ov.com_count;
	    	if(tdcb->srcs[x].ov.com_count + 2 >= tdcb->srcs[x].ov.com_size) {
	    	    if(internal_DCB_resize_cl(&tdcb->srcs[x].ov))
	    	    	return MEM_ERROR;
	    	}
	    	tdcb->srcs[x].ov.src_id[index] = x;
	    	tdcb->srcs[x].ov.command[index].offset = sdcb->srcs[scl->src_id[com_offset]].ov.command[cur->offset].offset + seek;
	    	tdcb->srcs[x].ov.command[index].len = tmp_len;
	    	tdcb->srcs[x].ov.com_count++;
	    	if(DCB_rec_copy_from_DCB_src(tdcb, &tdcb->srcs[x].ov, sdcb,
	    	    &sdcb->srcs[scl->src_id[com_offset]].ov,
	    	    translation_map,
	    	    scl->command[com_offset].offset + 1,
	    	    seek,
	    	    tmp_len)){
	    	    return MEM_ERROR;
	    	}
	    	tcl->command[tcl->com_count].offset = index;
	    	tcl->command[tcl->com_count].len = tdcb->srcs[x].ov.com_count - index;
	    	tcl->src_id[tcl->com_count] = x;
	    } else {
	    	tcl->src_id[tcl->com_count] = x;
		tcl->command[tcl->com_count].offset = cur->offset + seek;
	    	tcl->command[tcl->com_count].len = tmp_len;
	    }
	    tcl->com_count++;
	}
	len -= tmp_len;
	seek = 0;
	com_offset++;

	/* if at the end of scl, we best be at the end of this traversal. */
	assert(com_offset < scl->com_count || len == 0);
    }
    return 0;
}

int
DCB_add_overlay(CommandBuffer *dcb, off_u32 diff_src_pos, off_u32 len, int add_ov_id,
    off_u32 copy_src_pos, int ov_src_id)
{
    // error and sanity checks needed.
    overlay_chain *ov;
    ov = &dcb->srcs[add_ov_id].ov;
    if(ov->com_count + 1 >= ov->com_size) {
        if(((ov->command = (DCLoc *)realloc(
            ov->command, ov->com_size * 2 * 
            sizeof(DCLoc)))==NULL) ||
           ((ov->src_id = (unsigned char *)realloc(
            ov->src_id, ov->com_size * 2 *
            sizeof(unsigned char)))==NULL)) {
            return MEM_ERROR;
        }
        ov->com_size *= 2;
    }
    DCB_add_add(dcb, ov->com_count, 2, add_ov_id);
    dcb->reconstruct_pos += len - 2;
    ov->command[ov->com_count].offset = diff_src_pos;
    ov->command[ov->com_count].len = len;
    ov->src_id[ov->com_count] = add_ov_id;
    ov->com_count++;
    ov->command[ov->com_count].offset = copy_src_pos;
    ov->command[ov->com_count].len = len;
    ov->src_id[ov->com_count] = ov_src_id;
    ov->com_count++;
    return 0L;
}

int
DCB_add_add(CommandBuffer *buffer, unsigned long ver_pos, 
    unsigned long len, unsigned char src_id)
{
#ifdef DEV_VERSION
    v3printf("add v(%lu), l(%lu), id(%u), rpos(%lu)\n", ver_pos , len, src_id, 
	buffer->reconstruct_pos);
#endif
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	if(buffer->DCB.full.cl.com_count == buffer->DCB.full.cl.com_size &&
	    internal_DCB_resize_cl(&buffer->DCB.full.cl))
	    return MEM_ERROR;

	buffer->DCB.full.cl.src_id[buffer->DCB.full.command_pos] = src_id;
	buffer->DCB.full.cl.command[buffer->DCB.full.command_pos].offset = ver_pos;
	buffer->DCB.full.cl.command[buffer->DCB.full.command_pos].len = len;
	buffer->DCB.full.cl.com_count++;
	buffer->reconstruct_pos += len;
	DCBufferIncr(buffer);
    }
    return 0;
}

int
DCB_add_copy(CommandBuffer *buffer, unsigned long src_pos, 
    unsigned long ver_pos, unsigned long len, unsigned char src_id)
{
    unsigned long index;
#ifdef DEBUG_DCBUFFER
    buffer->total_copy_len += len;
#endif
#ifdef DEV_VERSION
    v3printf("copy s(%lu), v(%lu), l(%lu), rpos(%lu)\n", src_pos, ver_pos ,
	 len, buffer->reconstruct_pos);
#endif

    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	if(buffer->DCB.full.cl.com_count == buffer->DCB.full.cl.com_size) {
	    if(internal_DCB_resize_cl(&buffer->DCB.full.cl))
		return MEM_ERROR;
	}
	if(buffer->srcs[src_id].type & DCB_DCB_SRC) {

	    index = src_pos / buffer->srcs[src_id].src_ptr.dcb->s->quanta;
	    assert(index < buffer->srcs[src_id].src_ptr.dcb->s->index_size);
	    v0printf("index= %lu\n", index);
	    assert(src_pos + len >= buffer->srcs[src_id].src_ptr.dcb->s->ver_start[index]);
	    
	    if(DCB_rec_copy_from_DCB_src(buffer, &buffer->DCB.full.cl,
	    	buffer->srcs[src_id].src_ptr.dcb->src_dcb,
	    	&buffer->srcs[src_id].src_ptr.dcb->src_dcb->DCB.full.cl,
	    	buffer->srcs[src_id].src_ptr.dcb->src_map,
	    	buffer->srcs[src_id].src_ptr.dcb->s->index[index],
	    	src_pos - buffer->srcs[src_id].src_ptr.dcb->s->ver_start[index],
	    	len)) {
	    	return MEM_ERROR;
	    }
	    
	    /* ensure commands were copied in. */
	    assert(buffer->DCB.full.command_pos < buffer->DCB.full.cl.com_count);
	    
	    /* kind of dumb, but prefer to have DCBufferIncr still being used, should it ever do anything
	       fancy (check wise) */
	    buffer->DCB.full.command_pos = buffer->DCB.full.cl.com_count - 1;
	    
	} else {
	    buffer->DCB.full.cl.command[buffer->DCB.full.command_pos].offset = src_pos;
	    buffer->DCB.full.cl.command[buffer->DCB.full.command_pos].len = len;
	    buffer->DCB.full.cl.src_id[buffer->DCB.full.command_pos] = src_id;
	    buffer->DCB.full.cl.com_count++;
	}
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
    return 0;
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
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	return buffer->DCB.full.command_pos < buffer->DCB.full.cl.com_count;
    } else if(DCBUFFER_MATCHES_TYPE == buffer->DCBtype) {
	return (buffer->reconstruct_pos != buffer->ver_size);
    } else if(DCBUFFER_LLMATCHES_TYPE == buffer->DCBtype) {
	assert(DCB_LLM_FINALIZED & buffer->flags);
	return (buffer->reconstruct_pos != buffer->ver_size);
    }
    return 0;
}
	

void 
DCBufferFree(CommandBuffer *buffer)
{
    unsigned long x;
    if(DCBUFFER_FULL_TYPE == buffer->DCBtype) {
	free(buffer->DCB.full.cl.command);
	free(buffer->DCB.full.cl.src_id);
	buffer->DCB.full.cl.command = NULL;
	buffer->DCB.full.cl.src_id = NULL;
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
	if(buffer->srcs[x].flags & DCB_FREE_SRC_CFH) {
	    v2printf("cclosing src_cfh(%lu)\n", x);
	    cclose(buffer->srcs[x].src_ptr.cfh);
	    v2printf("freeing  src_cfh(%lu)\n", x);
	    free(buffer->srcs[x].src_ptr.cfh);
	}
	if(buffer->srcs[x].type & DCB_DCB_SRC) {
	    free_DCBSearch_index(buffer->srcs[x].src_ptr.dcb->s);
	    free(buffer->srcs[x].src_ptr.dcb);
	    buffer->srcs[x].src_ptr.dcb = NULL;
	}
	if(buffer->srcs[x].ov.com_size) {
	    free(buffer->srcs[x].ov.command);
	    free(buffer->srcs[x].ov.src_id);
	}
	buffer->srcs[x].ov.com_size = 0;
	buffer->srcs[x].ov.com_count = 0;
	buffer->srcs[x].ov.command = NULL;
	buffer->srcs[x].ov.src_id = NULL;
    }
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
#ifdef DEBUG_DCBUFFER
    buffer->total_copy_len = 0;
#endif

    buffer->src_count = 0;
    buffer->src_array_size = 4;
    if((buffer->srcs = (DCB_registered_src *)malloc(sizeof(DCB_registered_src) * buffer->src_array_size))==NULL) {
	return MEM_ERROR;
    }
    if(type == DCBUFFER_FULL_TYPE) {
	buffer->DCB.full.cl.com_count = 0;
	buffer->DCB.full.cl.com_size = buffer_size;

	if((buffer->DCB.full.cl.src_id = (unsigned char *)malloc(sizeof(unsigned char) * buffer_size))
	    == NULL){
	    return MEM_ERROR;
	} else if((buffer->DCB.full.cl.command = (DCLoc *)malloc(sizeof(DCLoc) *
	    buffer_size)) == NULL) {
	    return MEM_ERROR;
	}

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
internal_DCB_resize_cl(command_list *cl)
{
    assert(cl);
    if((cl->src_id = (unsigned char *)realloc(cl->src_id, sizeof(unsigned char) *
    	cl->com_size * 2)) == NULL 
    	||
    	(cl->command = (DCLoc *)realloc(cl->command, sizeof(DCLoc) * 
    	cl->com_size * 2)) == NULL) {
    	return MEM_ERROR;
    }
    cl->com_size *= 2;
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

DCBSearch *
create_DCBSearch_index(CommandBuffer *dcb)
{
    unsigned long pos, ver_pos, dpos, tmp_len;
    DCBSearch *s;

    if(dcb->DCBtype != DCBUFFER_FULL_TYPE)
    	return NULL;

    if(! dcb->ver_size) 
    	return NULL;

    s = malloc(sizeof(DCBSearch));

    if(s == NULL) 
    	return NULL;

    // basically, take a rough guess at the avg command len, use it to determine the divisor, then adjust index_size
    // to not allocate uneeded space (due to rounding of original index_size and quanta)
    if(dcb->DCB.full.cl.com_count < 2)
    	s->index_size = 1;
    else
	s->index_size = ceil(dcb->DCB.full.cl.com_count / 2);
    v0printf("index_size = %lu\n", s->index_size);
    s->quanta = ceil(dcb->ver_size / s->index_size);
    s->index_size = ceil(dcb->ver_size / s->quanta);
    s->index = (unsigned long *)malloc(sizeof(unsigned long) * s->index_size);

    if(s->index == NULL) {
	free(s);
	return NULL;
    }

    s->ver_start = (off_u64 *)malloc(sizeof(off_u64) * s->index_size);
    if(s->ver_start == NULL) {
    	free(s->index);
    	free(s);
    	return NULL;
    }
    pos=0;
    dpos = 0;
    ver_pos = 0;
    while(dpos < dcb->DCB.full.cl.com_count) {
	if(dcb->srcs[dcb->DCB.full.cl.src_id[dpos]].ov.com_count) {
	    tmp_len = dcb->srcs[dcb->DCB.full.cl.src_id[dpos]].ov.command[dcb->DCB.full.cl.command[dpos].offset].len;
	} else {
	    tmp_len = dcb->DCB.full.cl.command[dpos].len;
	}
	ver_pos += tmp_len;
    	while(ver_pos > pos * s->quanta && pos < s->index_size) {
    	    s->index[pos] = dpos;
    	    s->ver_start[pos] = ver_pos - tmp_len;
    	    pos++;
    	}
    	dpos++;
    }
    return s;    
}

void
free_DCBSearch_index(DCBSearch *s)
{
    if(s) {
    	free(s->index);
    	free(s->ver_start);
    	free(s);
    }
}

void
tfree(void *p)
{
    if(tfree != NULL) {
    	free(p);
    }
}
