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
#include <string.h>
#include "dcbuffer.h"
#include "bit-functions.h"
#include "bsdiff.h"

unsigned long
bsdiff_overlay_read(DCommand *dc, unsigned long pos, unsigned char *buff, unsigned long len, cfile *out_cfh)
{
    return 0L;
}

unsigned long 
bsdiff_overlay_copy(DCommand *dc, 
    cfile *out_cfh)
{
    unsigned char buff1[CFILE_DEFAULT_BUFFER_SIZE];
    unsigned char buff2[CFILE_DEFAULT_BUFFER_SIZE];
    cfile *src_cfh, *diff_cfh;
    DCB_ptr dcb;
    off_u32 len, src_pos;
    unsigned int x;
    dcb = dc->src_dcb;

    src_cfh = dcb->srcs[dc->src_id -1].cfh;
    diff_cfh = dcb->srcs[dc->src_id].cfh;
    len = dc->data.len;

    assert(src_cfh != NULL);
    assert(diff_cfh != NULL);

/*    src_offsets = (off_u32 *)dcb->extra_patch_data;
    if(src_offsets[dcb->DCB.full.add_index -1] != 
	cseek(src_cfh, 	src_offsets[dcb->DCB.full.add_index -1], CSEEK_FSTART)) {
	    return EOF_ERROR;
	return 0;
    } else if(dc->data.src_pos != cseek(diff_cfh, dc->data.src_pos, CSEEK_FSTART)) {
	return 0;
    }
*/
    src_pos = dcb->DCB.full.lb_start[dc->cmd_pos -1].offset;
    if(src_pos != cseek(src_cfh, src_pos, CSEEK_FSTART) ||
	dc->data.src_pos != cseek(diff_cfh, dc->data.src_pos, CSEEK_FSTART)) {
	return 0L;
    }
    while(len) {
	x = MIN(CFILE_DEFAULT_BUFFER_SIZE, len);
	if(x!=cread(src_cfh, buff1, x)) {
	    return dc->data.len - len;
	}
	if(x!=cread(diff_cfh, buff2, x)) {
	    return dc->data.len - len;
	}	
	while(x) {
	    buff1[x - 1] = ((buff1[x - 1] + buff2[x - 1]) & 0xff);
	    x--;
	}
	x = MIN(CFILE_DEFAULT_BUFFER_SIZE, len);
	if(x!=cwrite(out_cfh, buff1, x)) {
	    return dc->data.len - len;
	}
	len -= x;
    }
    return dc->data.len - len;
}


unsigned int
check_bsdiff_magic(cfile *patchf)
{
    unsigned char buff[BSDIFF_MAGIC_LEN];
    cseek(patchf, 0, CSEEK_FSTART);
    if(BSDIFF_MAGIC_LEN != cread(patchf, buff, BSDIFF_MAGIC_LEN)) {
	return 0;
    }
    if(memcmp(buff, BSDIFF4_MAGIC, BSDIFF_MAGIC_LEN)!=0 && 
	memcmp(buff, BSDIFF3_MAGIC, BSDIFF_MAGIC_LEN)!=0 && 
	memcmp(buff, BSDIFF_QS_MAGIC, BSDIFF_MAGIC_LEN)!=0) {
	return 0;
    }
    return 2;
}

/* note this currently only supports u32, no u64. yet */
signed int 
bsdiffReconstructDCBuff(cfile *ref_cfh, cfile *patchf, CommandBuffer *dcbuff)
{
    cfile ctrl_cfh, *diff_cfh, *extra_cfh;
//  following variables are related to allowing conversion of bsdiff formats, 
//  once a reference file option is added to convert_delta (if ever)
/*    cfile_window *cfw;
    unsigned char processing_add;
    off_u32 add_start;
*/
    int diff_id, extra_id, ref_id, diff_src_id;
    unsigned char ver;
    unsigned char buff[32];
    off_u32 len1, len2, diff_offset, extra_offset;
    off_s32 seek;
    off_u32 diff_len, ctrl_len;
    off_u32 ver_size;
    off_u32 ver_pos, src_pos;
    if(cread(patchf, buff, 32)!=32) {
	return EOF_ERROR;
    }
    if(memcmp(buff, BSDIFF4_MAGIC, BSDIFF_MAGIC_LEN)==0) {
	ver = 4;
    } else if (memcmp(buff, BSDIFF3_MAGIC, BSDIFF_MAGIC_LEN) == 0 || 
	memcmp(buff, BSDIFF_QS_MAGIC, BSDIFF_MAGIC_LEN) == 0) {
	ver = 3;
    } else {
	return PATCH_CORRUPT_ERROR;
    }

    if((diff_cfh = (cfile *)malloc(sizeof(cfile)))==NULL ||
	(ver == 4 && (extra_cfh = (cfile *)malloc(sizeof(cfile)))==NULL)) {
	return MEM_ERROR;
    }
    ctrl_len = readUBytesLE(buff + 8, 4);
    diff_len = readUBytesLE(buff + 16, 4);
    ver_size = readUBytesLE(buff + 24, 4);
    v1printf("start=32, ctrl_len=%lu, diff_len=%lu, ver_size=%lu\n", 
	ctrl_len, diff_len, ver_size);
    if(copen_child_cfh(&ctrl_cfh, patchf, 32, ctrl_len + 32,
	BZIP2_COMPRESSOR, CFILE_RONLY)) {
	return MEM_ERROR;
    } else if (copen_child_cfh(diff_cfh, patchf, ctrl_len + 32, 
	diff_len + ctrl_len + 32, BZIP2_COMPRESSOR, 
	CFILE_RONLY)) {
	return MEM_ERROR;
    } else if(ver  == 4 && copen_child_cfh(extra_cfh, patchf, 
	ctrl_len + diff_len + 32, cfile_len(patchf),
	BZIP2_COMPRESSOR, CFILE_RONLY)) {
	return MEM_ERROR;
    }
    
    ref_id = DCB_REGISTER_COPY_SRC(dcbuff, ref_cfh, NULL, 0);
    DCB_register_overlay_srcs(dcbuff, &diff_src_id, ref_cfh, NULL, NULL, 0,
	&diff_id, diff_cfh, &bsdiff_overlay_read, &bsdiff_overlay_copy, DCB_FREE_SRC_CFH);
//    diff_id = DCB_REGISTER_ADD_SRC(dcbuff, diff_cfh, &bsdiff_overlay_add, 1);
    if(ver == 4) {
	extra_id = DCB_REGISTER_ADD_SRC(dcbuff, extra_cfh, NULL, 1);
    }
    
    ver = (ver -1) *8;
    src_pos = ver_pos = 0;
    diff_offset = extra_offset = 0;
    len2 = 0;
    while(cread(&ctrl_cfh, buff, ver)==ver) {
	len1 = readUBytesLE(buff, 4);
	if(ver > 16) {
	    len2 = readUBytesLE(buff + 8, 4);
	    seek = readUBytesLE(buff + 16, 4);
	    if(buff[23] & 0x80) {
		seek = -seek;
	    }
	    v2printf("len1(%lu), len2(%lu), seek(%ld)\n", len1, len2, seek);
	} else {
	    seek = readUBytesLE(buff + 8, 4);
	    if(buff[15] & 0x80) {
		seek = -seek;
	    }
	    v2printf("len1(%lu), seek(%ld)\n", len1, seek);
	}
 	if(len1) {
/*	    cfw = expose_page(diff_cfh);
	    add_start = cfw->pos + cfw->offset;
	    processing_add = 0;

	    assert(diff_offset == cfw->offset + cfw->pos);
	    while(len1 + diff_offset != cfw->offset + cfw->pos) {
		if(cfw->pos == cfw->end) {
		    cfw = next_page(diff_cfh);
		    if(cfw->end == 0) {
			return EOF_ERROR;
		    }
		}


		if(cfw->buff[cfw->pos]==0 && processing_add) {
		    DCB_add_overlay(dcbuff, src_pos, diff_src_id, diff_offset, len1, diff_id);
		    DCB_add_add(dcbuff, add_start, cfw->offset +
			cfw->pos - add_start, diff_id);
		    processing_add = 0;
		    add_start = cfw->pos + cfw->offset;
		} else if(cfw->buff[cfw->pos]!=0 && processing_add==0){ 
		    DCB_add_copy(dcbuff, add_start - diff_offset + src_pos, 0,//dcbuff->reconstruct_pos, 
			cfw->offset + cfw->pos - add_start, ref_id);
		    processing_add=1;
		    add_start = cfw->pos + cfw->offset;
		    src_offsets[src_offset_count++] = cfw->pos + cfw->offset - diff_offset + src_pos;
		    if(src_offset_size == src_offset_count) {
			src_offset_size += 1000;
			if((src_offsets = (off_u32 *)realloc(src_offsets,
			    sizeof(off_u32) * src_offset_size))==NULL) {
			    return MEM_ERROR;
			}
		    }
		}
//		DCB_add_copy(dcbuff, src_pos, ver_pos, len1, diff_src_id);
//		DCB_add_overlay(dcbuff, diff_offset, len1, diff_id);
		DCB_add_overlay(dcbuff, src_pos, diff_src_id, diff_offset, len1, diff_id);
		cfw->pos++;
	    }
	    if(processing_add) {
		DCB_add_add(dcbuff, add_start, cfw->pos + cfw->offset  - add_start, diff_id);
	    } else {
		DCB_add_copy(dcbuff, add_start - diff_offset + src_pos, 0,//dcbuff->reconstruct_pos, 
		    diff_offset + len1 - add_start, ref_id);
	    }

*/
	    DCB_add_overlay(dcbuff, src_pos, diff_src_id, diff_offset, len1, diff_id);
 
 	    diff_offset += len1;
 	    src_pos += len1;
 	    ver_pos += len1;
 	}
	if(len2) {
/*	    src_offsets[src_offset_count++] = 0;
	    if(src_offset_size == src_offset_count) {
		src_offset_size += 1000;
		if((src_offsets = (off_u32 *)realloc(src_offsets,
		    sizeof(off_u32) * src_offset_size))==NULL) {\
		    return MEM_ERROR;
		}
	    }
*/
	    DCB_add_add(dcbuff, extra_offset, len2, extra_id);
	    extra_offset+=len2;
	    ver_pos += len2;
	}
	src_pos += seek;
	assert(ver_pos == dcbuff->reconstruct_pos);
	assert(ver_pos <= ver_size);
    }
    v1printf("ver_pos=%lu, size=%lu, extra_pos=%lu, diff_pos=%lu, ctrl_pos=%lu, recon=%lu\n", ver_pos, ver_size, 
	extra_offset, diff_offset, ctrl_cfh.data.pos + ctrl_cfh.data.offset, 
	dcbuff->reconstruct_pos);
    if(ver_pos != ver_size) {
	printf("error detected, aborting...\n");
	return PATCH_CORRUPT_ERROR;
    }
//    DCB_REGISTER_EXTRA_PATCH_DATA(dcbuff, src_offsets);
    return 0;
}

signed int 
bsdiffEncodeDCBuffer(CommandBuffer *buffer, cfile *ver_cfh, cfile *out_cfh)
{
    return 0;
}


