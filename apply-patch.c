/*
  Copyright (C) 2003-2005 Brian Harring

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
#include "defs.h"
#include "cfile.h"
#include "dcbuffer.h"
#include "apply-patch.h"
#include "defs.h"

#define ap_printf(fmt...) \
fprintf(stderr, "%s: ",__FILE__);   \
fprintf(stderr, fmt);
#define MAX(x,y) ((x) > (y) ? (x) : (y))

int
cmp_dcloc_match(const void *vd1, const void *vd2)
{
    #define v_cl(v)	((DCLoc_match *)(v))

    return v_cl(vd1)->src_pos < v_cl(vd2)->src_pos ? -1 : 
    	v_cl(vd1)->src_pos > v_cl(vd2)->src_pos ? 1 :
    	v_cl(vd1)->len < v_cl(vd2)->len ? -1 : 
    	v_cl(vd1)->len > v_cl(vd2)->len ? 1 : 
    	0;
}

int 
reconstructFile(CommandBuffer *dcbuff, cfile *out_cfh, int reorder_for_seq_access, unsigned long max_buff_size)
{
    DCommand *dc = NULL;
    assert(DCBUFFER_FULL_TYPE == dcbuff->DCBtype);
    command_list *cl, **ov_cl, **norm_cl;
    unsigned long x, ov_count, norm_count;
    
    DCBufferReset(dcbuff);

    assert(reorder_for_seq_access == 0 || CFH_IS_SEEKABLE(out_cfh) || 1);
    if(reorder_for_seq_access) {

	v1printf("collapsing\n");
	if((cl = DCB_collapse_commands(dcbuff)) == NULL)
	    return MEM_ERROR;

	if((ov_cl = (command_list **)malloc(sizeof(command_list *) * dcbuff->src_count))==NULL)
	    return MEM_ERROR;
	if((norm_cl = (command_list **)malloc(sizeof(command_list *) * dcbuff->src_count))==NULL)
	    return MEM_ERROR;
	
	for(x=0, ov_count=0, norm_count=0; x < dcbuff->src_count; x++) {
	    if(dcbuff->srcs[x].ov == NULL) {
	    	norm_cl[norm_count] = cl + x;
	    	norm_count++;
	    } else {
	    	ov_cl[ov_count] = cl + x;
	    	ov_count++;
	    }
	}
	
	DCB_free_commands(dcbuff);

	for(x=0; x < norm_count; x++) {
	    v1printf("processing src %u: %lu commands.\n", norm_cl[x] - cl, norm_cl[x]->com_count);
	    if(norm_cl[x]->com_count) {
		qsort(norm_cl[x]->full_command, norm_cl[x]->com_count, sizeof(DCLoc_match), cmp_dcloc_match);
	    	if(read_seq_write_rand(norm_cl[x], dcbuff->srcs + (norm_cl[x] - cl), 0, out_cfh, max_buff_size))
	    	    return IO_ERROR;
	    }
	    CL_free(norm_cl[x]);
	}

	for(x=0; x < ov_count; x++) {
	    v1printf("processing overlay src %u: %lu commands.\n", ov_cl[x] - cl, ov_cl[x]->com_count);
	    if(ov_cl[x]->com_count) {
		qsort(ov_cl[x]->full_command, ov_cl[x]->com_count, sizeof(DCLoc_match), cmp_dcloc_match);
	    	if(read_seq_write_rand(ov_cl[x], dcbuff->srcs + (ov_cl[x] - cl), 1, out_cfh, max_buff_size))
	    	    return IO_ERROR;
	    }
	    CL_free(ov_cl[x]);
	}
	
	free(cl);
	free(ov_cl);
	free(norm_cl);

    } else {
	if((dc = (DCommand *)malloc(sizeof(DCommand))) == NULL) {
	    return MEM_ERROR;
	}
	while(DCB_commands_remain(dcbuff)) {
	    DCB_get_next_command(dcbuff, dc);
	    if(dc->data.len != copyDCB_add_src(dcbuff, dc, out_cfh)) {
		return EOF_ERROR;
	    }
	}
	free(dc);
    }
    return 0;
}

int
read_seq_write_rand(command_list *cl, DCB_registered_src *r_src, unsigned char is_overlay, cfile *out_cfh, unsigned long buf_size)
{
    unsigned char *buf;
    unsigned char *p;
    unsigned long x, start=0, end=0, len=0;
    unsigned long max_pos = 0, pos = 0;
    unsigned long offset;
    signed long tmp_len;
    dcb_src_read_func read_func;
    cfile_window *cfw;
    u_dcb_src u_src;
    
    #define END_POS(x) ((x).src_pos + (x).len)
    pos = 0;
    max_pos = 0;

    if(is_overlay) {
	read_func = r_src->mask_read_func;
//	read_func = r_src->read_func;
    } else {
	read_func = r_src->read_func;
    }
    assert(read_func != NULL);
    u_src = r_src->src_ptr;
    if(0 != cseek(u_src.cfh, 0, CSEEK_FSTART)) {
	ap_printf("cseeked failed: bailing, io_error 0\n");
	return IO_ERROR;
    }

    if((buf = (unsigned char *)malloc(buf_size)) == NULL) {
	return MEM_ERROR;
    }

    // we should *never* go backwards
    u_src.cfh->state_flags |= CFILE_FLAG_BACKWARD_SEEKS;	

    while(start < cl->com_count) {
	if(pos < cl->full_command[start].src_pos) {
	    pos = cl->full_command[start].src_pos;
	    max_pos = END_POS(cl->full_command[start]);
	} else {
	    while(start < cl->com_count && pos > cl->full_command[start].src_pos) {
		start++;
	    }
	    if(start == cl->com_count)
		continue;
	    pos = cl->full_command[start].src_pos;
	    max_pos = MAX(max_pos, END_POS(cl->full_command[start]));
	}
	if(end < start) {
	    end = start;
	}
	while(end < cl->com_count && cl->full_command[end].src_pos < max_pos) {
	    max_pos = MAX(max_pos, END_POS(cl->full_command[end]));
	    end++;
	}
	if(pos == max_pos) {
	    continue;
	}
	while(pos < max_pos) {
	    len = MIN(max_pos - pos, buf_size);
	    x = read_func(u_src, pos, buf, len);
//	    if(len < max_pos - pos)
//		v0printf("buffered %lu, max was %lu\n", len, max_pos - pos);
	    if(len != x){
		ap_printf("x=%lu, pos=%lu, len=%lu\n", x, pos, len);
		ap_printf("bailing, io_error 2\n");
		free(buf);
		return IO_ERROR;
	    }
	    for(x=start; x < end; x++) {
		offset = MAX(cl->full_command[x].src_pos, pos);
		tmp_len = MIN(END_POS(cl->full_command[x]), pos + len) - offset;
		    
		if(tmp_len > 0) { 
		    if(cl->full_command[x].ver_pos + (offset - cl->full_command[x].src_pos) !=
			cseek(out_cfh, cl->full_command[x].ver_pos + (offset - cl->full_command[x].src_pos),
			CSEEK_FSTART)) {
			ap_printf("bailing, io_error 3\n");
			free(buf);
			return IO_ERROR;
		    }
		    if(is_overlay) {
			p = buf + offset - pos;
			cfw = expose_page(out_cfh);
			if(cfw->write_end == 0) {
			    cfw->write_start = cfw->pos;
			}
			while(buf + offset - pos + tmp_len > p) {
			    if(cfw->pos == cfw->end) {
			        cfw->write_end = cfw->end;
			        cfw = next_page(out_cfh);
			        if(cfw->end == 0) {
			    	    ap_printf("bailing from applying overlay mask in read_seq_writ_rand\n");
				    free(buf);
			    	    return IO_ERROR;
			    	}
			    }
			    cfw->buff[cfw->pos] += *p;
			    p++;
			    cfw->pos++;
			}
			cfw->write_end = cfw->pos;
		    } else {
			if(tmp_len != cwrite(out_cfh, buf + offset - pos, tmp_len)) {
			    ap_printf("bailing, io_error 4\n");
			    free(buf);
			    return IO_ERROR;
			}
		    }
		}
	    }
	    pos += len;
	}
    }
    u_src.cfh->state_flags &= ~CFILE_FLAG_BACKWARD_SEEKS;
    free(buf);
    return 0;
}

