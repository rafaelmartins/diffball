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
cmp_DCommand_abbrev(const void *vd1, const void *vd2)
{
    DCommand_abbrev *d1, *d2;
    d1 = (DCommand_abbrev *)vd1;
    d2 = (DCommand_abbrev *)vd2;
//    return	d1->src_id != d2->src_id ? d1->src_id - d2->src_id :
//		d1->data.src_pos != d2->data.src_pos ? d1->data.src_pos - d2->data.src_pos :
//		d1->data.len - d2->data.len;
    return	d1->src_id != d2->src_id ? (d1->src_id < d2->src_id ? -1 : 1)  : 
		d1->data.src_pos != d2->data.src_pos ? d1->data.src_pos - d2->data.src_pos :
		d1->data.len - d2->data.len;
}

int 
reconstructFile(CommandBuffer *dcbuff, cfile *out_cfh, int reorder_for_seq_access)
{
    DCommand *dc = NULL;
    assert(DCBUFFER_FULL_TYPE == dcbuff->DCBtype);
    DCommand_abbrev *d1, *d2;
    unsigned long l1, l2;
    DCBufferReset(dcbuff);
    
    reorder_for_seq_access = 1;
    reorder_for_seq_access = 0;

    assert(reorder_for_seq_access == 0 || CFH_IS_SEEKABLE(out_cfh) || 1);
    if(reorder_for_seq_access) {
	DCB_collapse_commands(dcbuff, &d1, &l1, &d2, &l2);
	if(l1) 
	    qsort(d1, l1, sizeof(DCommand_abbrev), cmp_DCommand_abbrev);
	if(l2)
	    qsort(d2, l2, sizeof(DCommand_abbrev), cmp_DCommand_abbrev);
	ap_printf("calling read_seq w/ %lu, %lu\n", l1, l2);
	if(read_seq_write_rand(dcbuff, d1, l1 + l2, out_cfh)) {
	    return IO_ERROR;
	}
	free(d1);
    } else {
	if((dc = (DCommand *)malloc(sizeof(DCommand))) == NULL) {
	    return MEM_ERROR;
	}
	l1 = 0;
	while(DCB_commands_remain(dcbuff)) {
//	    ap_printf("processing command %lu\n", l1);
	    DCB_get_next_command(dcbuff, dc);
	    if(dc->data.len != copyDCB_add_src(dcbuff, dc, out_cfh)) {
		return EOF_ERROR;
	    }
	    l1++;
	}
	free(dc);
    }
    return 0;
}

int
//read_seq_write_rand(CommandBuffer *dcb, DCommand *dc_array, unsigned long array_size, cfile *out_cfh)
read_seq_write_rand(CommandBuffer *dcb, DCommand_abbrev *dc_array, unsigned long array_size, cfile *out_cfh)
{
    #define buf_size 0x10000
    unsigned char buf[buf_size];
    unsigned long ver_pos;
    unsigned char *p;
    unsigned long x, start=0, end=0, len=0;
    unsigned long max_pos = 0, pos = 0;
    unsigned long offset;
    signed long tmp_len;
    unsigned int src_id;
    unsigned char is_overlay = 0;
    DCB_registered_src *cur_src;
    dcb_src_read_func read_func;
    cfile_window *cfw;
    u_dcb_src u_src;
    #define END_POS(x) ((x).data.src_pos + (x).data.len)
    while(end < array_size) {
    	src_id = dc_array[end].src_id;
	is_overlay = (dcb->srcs[src_id].ov.com_count > 0);
	start = end;
	pos = dc_array[start].data.src_pos;
	max_pos = END_POS(dc_array[start]);
	while(start < array_size && (end == array_size || src_id == dc_array[end].src_id)) {
	    if(pos < dc_array[start].data.src_pos) {
		pos = dc_array[start].data.src_pos;
		max_pos = END_POS(dc_array[start]);
	    } else {
		while(start < array_size && pos > dc_array[start].data.src_pos) {
		    start++;
		}
		if(start == array_size)
		    continue;
		pos = dc_array[start].data.src_pos;
		max_pos = MAX(max_pos, END_POS(dc_array[start]));
	    }
	    if(end < start) {
		end = start;
	    }
	    while(end < array_size && dc_array[end].data.src_pos < max_pos && dc_array[end].src_id == dc_array[start].src_id) {
		max_pos = MAX(END_POS(dc_array[end]), max_pos);
		end++;
	    }
	    if(pos == max_pos) {
		continue;
	    }
	    u_src = dcb->srcs[src_id].src_ptr;
	    if(is_overlay) {
	    	read_func = dcb->srcs[src_id].mask_read_func;
	    } else {
		    read_func = dcb->srcs[src_id].read_func;
	    }
	    while(pos < max_pos) {
		len = MIN(max_pos - pos, buf_size);
		x=read_func(u_src, pos, buf, len);
		if(len != x){
		    ap_printf("x=%lu, len=%lu\n", x, len);
		    ap_printf("bailing, io_error 2\n");
		    return IO_ERROR;
		}
		printf("start=%lu, end=%lu\n", start, end);
		for(x=start; x < end; x++) {
		    offset = MAX(dc_array[x].data.src_pos, pos);
		    tmp_len = MIN(END_POS(dc_array[x]), pos + len) - offset;
		    printf("offset=%lu, tmp_len=%lu\n", offset, tmp_len);
		    if(tmp_len > 0) { 
			if(dc_array[x].data.ver_pos + (offset - dc_array[x].data.src_pos) != 
			    cseek(out_cfh, dc_array[x].data.ver_pos + (offset - dc_array[x].data.src_pos),
			    CSEEK_FSTART)) {
			    ap_printf("bailing, io_error 3\n");
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
			    	    	return IO_ERROR;
			    	    }
			    	}
			    	cfw->buff[cfw->pos] += *p;
			    	p++;
			    	cfw->pos++;
			    }
			    cfw->write_end = cfw->pos;
//			    ap_printf("finished w/ ov command\n");
			    
			} else {
			    if(tmp_len != cwrite(out_cfh, buf + offset - pos, tmp_len)) {
				ap_printf("bailing, io_error 4\n");
				return IO_ERROR;
			    }
			}
		    }
		}
		pos += len;
	    }
	} /* while end < array_size */
    }
    return 0;
}

