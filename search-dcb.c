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
#include "dcbuffer.h"
#include "search-dcb.h"
#include <math.h>

DCBSearch *
create_DCBSearch_index(CommandBuffer *dcb)
{
    unsigned long pos, ver_pos, dpos;
    DCBSearch *s;
    DCLoc *dptr;
    if(dcb->DCBtype != DCBUFFER_FULL_TYPE)
    	return NULL;
    if(! dcb->ver_size) 
    	return NULL;
    s = malloc(sizeof(DCBSearch));
    if(s == NULL) 
    	return NULL;

    // basically, take a rough guess at the avg command len, use it to determine the divisor, then adjust index_size
    // to not allocate uneeded space (due to rounding of original index_size and quanta)
    s->index_size = ceil(dcb->DCB.full.buffer_count / 2);
    s->quanta = ceil(dcb->ver_size / s->index_size);
    s->index_size = ceil(dcb->ver_size / s->quanta);
    s->index = (DCLoc **)malloc(sizeof(DCLoc *) * s->index_size);
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
    dptr = dcb->DCB.full.lb_start;
    ver_pos = 0;
    while(dpos < dcb->DCB.full.buffer_count) {
    	ver_pos += dptr[dpos].len;
    	while(ver_pos > pos * s->quanta && pos < s->index_size) {
    	    s->index[pos] = dptr + dpos;
    	    s->ver_start[pos] = ver_pos - dptr[dpos].len;
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

int
merge_version_buffers(CommandBuffer *src, CommandBuffer *ver, unsigned long merge_id, CommandBuffer *new)
{
    
    DCBSearch 		*s;
    DCLoc		*dcl;
    DCommand 		dc;
    #define  NOT_TRANSLATED	0xffffffff
    unsigned long 	*ver_map, *src_map;
    unsigned long 	x, pos, src_id;
    unsigned long 	count=0;

    if(src->DCBtype != DCBUFFER_FULL_TYPE) {
    	return -1;
    }

    if(DCBufferInit(new, ver->DCB.full.buffer_size, src->src_size, ver->ver_size, DCBUFFER_FULL_TYPE)) {
	free(new);
    	return MEM_ERROR;
    }
    s = create_DCBSearch_index(src);
    src_map = (unsigned long *)malloc(sizeof(unsigned long) * src->src_count);
    ver_map = (unsigned long *)malloc(sizeof(unsigned long) * ver->src_count);
    
    if(!s || !src_map || !ver_map) {
	DCBufferFree(new);
	//tfree(new);	
	free_DCBSearch_index(s);	
	tfree(src_map);
	tfree(ver_map);
    	return MEM_ERROR;
    }
       
    for(x=0; x < src->src_count; x++)
    	src_map[x] = NOT_TRANSLATED;
    for(x=0; x < ver->src_count; x++)
    	ver_map[x] = NOT_TRANSLATED;
    	
    DCBufferReset(ver);
    while(DCB_commands_remain(ver)) {
//	v0printf("processing command %lu\n", count);
    	DCB_get_next_command(ver, &dc);
	if(dc.src_id != merge_id) {
	    assert(dc.src_id < ver->src_count);
	    if(ver_map[dc.src_id] == NOT_TRANSLATED) {
	    	ver_map[dc.src_id] = DCB_register_src(new, dc.dcb_src->src_ptr.cfh,
	    	    dc.dcb_src->read_func, dc.dcb_src->copy_func, dc.dcb_src->flags, (dc.type & 0x1));
	    	dc.dcb_src->flags &=  ~DCB_FREE_SRC_CFH; 
	    }
    	    if(dc.type == DC_ADD) {
	    	DCB_add_add(new, dc.data.src_pos, dc.data.len, ver_map[dc.src_id]);
	    } else {
		DCB_add_copy(new, dc.data.src_pos, dc.data.ver_pos, dc.data.len,
		    ver_map[dc.src_id]);
	    }
	} else {
	    assert(dc.data.src_pos/s->quanta < s->index_size);
	    assert(s->ver_start[dc.data.src_pos/s->quanta] <= dc.data.src_pos);
//	    v0printf("copying from src\n");
	    dcl = s->index[dc.data.src_pos / s->quanta];
	    x = dc.data.src_pos - s->ver_start[dc.data.src_pos / s->quanta];
	    while(x >= dcl->len) {
		x -= dcl->len;
	    	dcl++;
	    }
	    pos = dc.data.ver_pos;
	    while(pos < dc.data.ver_pos + dc.data.len) {
		src_id = src->DCB.full.src_id[dcl - src->DCB.full.lb_start];

		if(src_map[src_id] == NOT_TRANSLATED) {
		    src_map[src_id] = DCB_register_src(new, src->srcs[src_id].src_ptr.cfh,
		    	src->srcs[src_id].read_func, src->srcs[src_id].copy_func, 
		    	src->srcs[src_id].flags, (src->srcs[src_id].type & 0x1));
		    src->srcs[src_id].flags &= ~DCB_FREE_SRC_CFH;
		}
		if((src->srcs[src_id].type & 0x1) == DC_ADD) {
	    	    DCB_add_add(new, dcl->offset + x,
	    	    	MIN(dcl->len - x, dc.data.len - (pos - dc.data.ver_pos)), src_map[src_id]);
	    	} else {
	    	    DCB_add_copy(new, dcl->offset + x, 
	    	    	pos, MIN(dcl->len - x, dc.data.len - (pos - dc.data.ver_pos)), src_map[src_id]);
		}
		pos += dcl->len - x;
		dcl++;
		x = 0;
	    }
	}
	count++;
    }
    free(ver_map);
    free(src_map);
    free_DCBSearch_index(s);
    return 0;
}

