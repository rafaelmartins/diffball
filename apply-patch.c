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
    return 	d1->type != d2->type ? d1->type - d2->type :
		d1->src_id != d2->src_id ? d1->src_id - d2->src_id :
		d1->data.src_pos != d2->data.src_pos ? d1->data.src_pos - d2->data.src_pos :
		d1->data.len - d2->data.len;
}

int 
reconstructFile(CommandBuffer *dcbuff, cfile *out_cfh)
{
/*    DCommand *dc = NULL, *dc_array = NULL;
    unsigned long array_size = 0;
    assert(DCBUFFER_FULL_TYPE == dcbuff->DCBtype);
    if(CFH_IS_SEEKABLE(out_cfh)) {
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
	    if(dc - dc_array >= array_size) {
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
    } else {
	if((dc = (DCommand *)malloc(sizeof(DCommand))) == NULL) {
	    return MEM_ERROR;
	}
	dc_array = dc;
//    }
*/
    DCommand *dc, dc2;
    dc = &dc2;
    DCBufferReset(dcbuff);
//    while(1==1) {
	
    while(DCB_commands_remain(dcbuff)) {
	DCB_get_next_command(dcbuff, dc);
//	if(cseek(out_cfh, dc->data.ver_pos, CSEEK_FSTART)!=dc->data.ver_pos) {
//	    abort();
//	}
  
	if(DC_COPY == dc->type) {
	    v2printf("copy command, offset(%lu), len(%lu)\n",
		dc->data.src_pos, dc->data.len);
	    if(dc->data.len != copyDCB_copy_src(dcbuff, dc, out_cfh)) {
		return EOF_ERROR;
	    }
	} else {
	    v2printf("add command, offset(%lu), len(%lu)\n", 
		dc->data.src_pos, dc->data.len);
	    if(dc->data.len != copyDCB_add_src(dcbuff, dc, out_cfh)) {
		return EOF_ERROR;
	    }
	}
    }
/*    if(dc_array != NULL) {
	free(dc_array);
    } else {
	free(dc);
    }*/
    return 0;
}

#define MAX(x,y) ((x) > (y) ? (x) : (y))
int
read_seq_write_rand(CommandBuffer *dcb, DCommand *dc_array, unsigned long array_size, cfile *out_cfh)
{
    #define buf_size 0x10000
    unsigned long x, pos=0, start=0, end=0, len=0;
    unsigned long max_pos = 0;
    unsigned char buf[buf_size];
    cfile *cfh = NULL;
    start = dc_array[0].data.src_pos;
    #define END_POS(x) ((x).data.src_pos + (x).data.len)
    while(start < array_size) {
	if(pos < dc_array[start].data.src_pos) {
	    pos = dc_array[start].data.src_pos;
	    max_pos = END_POS(dc_array[start]);
	} else {
	    while(pos > dc_array[start].data.src_pos && start < array_size) {
		start++;
	    }
	}
	if(end < start) {
	    end = start;
	}
	while(dc_array[end].data.src_pos < max_pos && end < array_size) {
	    max_pos = MAX(END_POS(dc_array[end]), max_pos);
	    end++;
	}
	if(pos == max_pos) {
	    continue;
	}
	while(pos < max_pos) {
//	    if(dc_array[start].type == DC_ADD) {
//		cfh = dcb->add_
	    for(x=start; x < end; x++) {
		len = MAX(max_pos - pos, buf_size);
		
	    }
	}
    }
}

