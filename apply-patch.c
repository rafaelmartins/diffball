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

int
cmp_DCommand(const void *vd1, const void *vd2)
{
    DCommand *d1, *d2;
    d1 = (DCommand *)vd1;
    d2 = (DCommand *)vd2;
    return	d1->src_id != d2->src_id ? d1->src_id - d2->src_id :
		d1->data.src_pos != d2->data.src_pos ? d1->data.src_pos - d2->data.src_pos :
		d1->data.len - d2->data.len;
}

int 
reconstructFile(CommandBuffer *dcbuff, cfile *out_cfh, int reorder_for_seq_access)
{
    DCommand *dc = NULL, *dc_array = NULL;
    unsigned long array_size = 0;
    assert(DCBUFFER_FULL_TYPE == dcbuff->DCBtype);
    reorder_for_seq_access = 1;
    assert(reorder_for_seq_access == 0 || CFH_IS_SEEKABLE(out_cfh) || 1);
    if(reorder_for_seq_access) {

	//copy the buffer out for reorganization
	array_size = 1024;
	if((dc_array = (DCommand *)malloc(sizeof(DCommand) * array_size))
	     == NULL) {
	    return MEM_ERROR;
	}
	dc = dc_array;
	DCBufferReset(dcbuff);
	while(DCB_commands_remain(dcbuff)) {
	    DCB_get_next_command(dcbuff, dc);
	    dc++;
	    if((unsigned long)(dc - dc_array) >= array_size) {
		if((dc_array = (DCommand *)realloc(dc_array, 
		   sizeof(DCommand) * array_size * 2)) == NULL) {
		   return MEM_ERROR;
		}
		dc = dc_array + array_size;
		array_size *= 2;
	    }
	}
	// shirk off unused memory
	array_size = dc - dc_array;
	if((dc_array = (DCommand *)realloc(dc_array, sizeof(DCommand) *
	    array_size))==NULL) {
	    return MEM_ERROR;
	}
	qsort(dc_array, array_size, sizeof(DCommand), cmp_DCommand);
	dc = dc_array;
	if(read_seq_write_rand(dcbuff, dc_array, array_size, out_cfh)){
	    return IO_ERROR;
	}
	free(dc_array);
    } else {
	if((dc = (DCommand *)malloc(sizeof(DCommand))) == NULL) {
	    return MEM_ERROR;
	}
	DCBufferReset(dcbuff);
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

#define MAX(x,y) ((x) > (y) ? (x) : (y))
int
read_seq_write_rand(CommandBuffer *dcb, DCommand *dc_array, unsigned long array_size, cfile *out_cfh)
{
    #define buf_size 0x10000
    unsigned char buf[buf_size];
    unsigned long x, start=0, end=0, len=0;
    unsigned long max_pos = 0, pos = 0;
    unsigned long offset;
    signed long tmp_len;
    cfile *cfh = NULL;
    unsigned long old_id = 0;
    #define END_POS(x) ((x).data.src_pos + (x).data.len)
    while(end < array_size) {
	old_id = dc_array[end].src_id;
	start = end;
	pos = dc_array[start].data.src_pos;
	max_pos = END_POS(dc_array[start]);
	while(start < array_size && (end == array_size || old_id == dc_array[end].src_id)) {
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
	    cfh = dcb->srcs[dc_array[start].src_id].cfh;
	    if(pos != cseek(cfh, pos, CSEEK_FSTART)) {
		v0printf("bailing, io_error 1\n");
		return IO_ERROR;
	    }
	    while(pos < max_pos) {
		len = MIN(max_pos - pos, buf_size);
		x=cread(cfh, buf, len);
		if(len != x){
		    v0printf("x=%lu, len=%lu\n", x, len);
		    v0printf("bailing, io_error 2\n");
		    return IO_ERROR;
		}
		for(x=start; x < end; x++) {
		    offset = MAX(dc_array[x].data.src_pos, pos);
		    tmp_len = MIN(END_POS(dc_array[x]), pos + len) - offset;
		    if(tmp_len > 0) { // && dc_array[x].data.src_pos <= pos) {
			if(dc_array[x].data.ver_pos + offset - dc_array[x].data.src_pos != 
			    cseek(out_cfh, dc_array[x].data.ver_pos + offset - dc_array[x].data.src_pos,
			    CSEEK_FSTART)) {
			    v0printf("bailing, io_error 3\n");
			    return IO_ERROR;
			}
			if(tmp_len != cwrite(out_cfh, buf + offset - pos, tmp_len)) {
			    v0printf("bailing, io_error 4\n");
			    return IO_ERROR;
			}
		    }
		}
		pos += len;
	    }
	} /* while end < array_size */
    }
    return 0;
}

