/*
  Copyright (C) 2003-2004 Brian Harring

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
bsdiff_overlay_read(u_dcb_src uc, unsigned long pos, unsigned char *buff, unsigned long len)
{
    return 0L;
}

unsigned long
bsdiff_apply_overlay(cfile *cfh, unsigned long src_pos, unsigned char *buff, 
    unsigned long len)
{
    cfile_window *scfw;
    unsigned char *p;
    p = buff;
    scfw = expose_page(cfh);
    while(p != buff + len) {
    	if(scfw->pos == scfw->end) {
    	    scfw = next_page(cfh);
    	    if (scfw->end == 0)
    	    	return p - buff;
    	}
    	*p += scfw->buff[scfw->pos];
    	p++; 
    	scfw->pos++;
    }
    return p - buff;
}

unsigned long 
bsdiff_overlay_copy(DCommand *dc, 
    cfile *out_cfh)
{
    #define MIN(x,y) ((x) < (y) ? (x) : (y))
    cfile_window *cfw;
    cfile_window *ocfw;
    unsigned long bytes_wrote = 0;
    unsigned long com_len;
    unsigned long tmp_len;
    unsigned long commands_read = 0;
    DCLoc *dptr;  DCB_registered_src *dsrc;
    overlay_chain *ov;
    ov = dc->dcb_src->ov;

    // error checking...
    v3printf("processing src(%lu), len(%lu), ver(%lu)\n", dc->data.src_pos, dc->data.len, dc->data.ver_pos);
    cflush(out_cfh);
    if(dc->data.src_pos != cseek(dc->dcb_src->src_ptr.cfh, dc->data.src_pos, CSEEK_FSTART)) {
    	return 0L;
    }
    cfw = expose_page(dc->dcb_src->src_ptr.cfh);
    ocfw = expose_page(out_cfh);
    commands_read = 1;
    while(dc->ov_len > commands_read) {
    	com_len = 0;
	dptr = ov->command + dc->ov_offset + commands_read;
	dsrc = &dc->dcb_ptr->srcs[ov->src_id[dc->ov_offset + commands_read]];
	while(dptr->len > com_len) {
	    tmp_len = MIN(dptr->len - com_len, ocfw->size);
	    if(tmp_len != dsrc->read_func(dsrc->src_ptr, 
	    	dptr->offset + com_len, ocfw->buff, tmp_len)) {
		// note thate just the previous byte_count is returned.
		// that number contains the copyed (eg read AND overlayed)
		// don't want the data to be considered valid, hence leavng ocfw->pos as 0.
		printf("bailing\n");
	    	return bytes_wrote;
	    }
	    while(ocfw->pos < tmp_len) {
	    	if(cfw->pos < cfw->end) {
	    	    ocfw->buff[ocfw->pos] += cfw->buff[cfw->pos];
	    	    cfw->pos++;
	    	    ocfw->pos++;
	    	} else {
		    cfw = next_page(dc->dcb_src->src_ptr.cfh);
		    if(cfw->end == 0) {
		    	return bytes_wrote + ocfw->pos;
		    }
		}
	    }
	    ocfw->write_end = tmp_len;
	    cflush(out_cfh);
	    com_len += tmp_len;
	    bytes_wrote += tmp_len;
	}
	commands_read++;
    }
    return bytes_wrote;
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
bsdiffReconstructDCBuff(unsigned char src_id, cfile *patchf, CommandBuffer *dcbuff)
{
    cfile ctrl_cfh, *diff_cfh, *extra_cfh = NULL;
//  following variables are related to allowing conversion of bsdiff formats, 
//  once a reference file option is added to convert_delta (if ever)
//  note, this functionality is removed right now.  pull it from cvs when needed

    int diff_id, extra_id = -1, ref_id;
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
    dcbuff->ver_size = ver_size;
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
    
    ref_id = src_id;
    diff_id = DCB_register_overlay_src(dcbuff, diff_cfh,  &bsdiff_overlay_read, 
    	&bsdiff_overlay_copy, NULL, (unsigned char)DCB_FREE_SRC_CFH);
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
	    DCB_add_overlay(dcbuff, diff_offset, len1, diff_id, src_pos, ref_id);
 
 	    diff_offset += len1;
 	    src_pos += len1;
 	    ver_pos += len1;
 	}
	if(len2) {
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
    cclose(&ctrl_cfh);
    return 0;
}

signed int 
bsdiffEncodeDCBuffer(CommandBuffer *buffer, cfile *ver_cfh, cfile *out_cfh)
{
    return 0;
}


