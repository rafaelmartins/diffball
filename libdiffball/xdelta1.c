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
#include <diffball/defs.h>
#include <string.h>
#include <cfile.h>
#include <diffball/bit-functions.h>
#include <diffball/xdelta1.h>

unsigned int
check_xdelta1_magic(cfile *patchf)
{
	unsigned char buff[XDELTA_MAGIC_LEN];
	cseek(patchf, 0, CSEEK_FSTART);
	if(XDELTA_MAGIC_LEN != cread(patchf, buff, XDELTA_MAGIC_LEN)) {
		return 0;
	}
	if(memcmp(buff, XDELTA_110_MAGIC, XDELTA_MAGIC_LEN)==0) {
		return 2;
	} else if (memcmp(buff, XDELTA_104_MAGIC, XDELTA_MAGIC_LEN)==0) {
		return 1;
	} else if (memcmp(buff, XDELTA_100_MAGIC, XDELTA_MAGIC_LEN)==0) {
		return 1;
	} else if (memcmp(buff, XDELTA_020_MAGIC, XDELTA_MAGIC_LEN)==0) {
		return 1;
	} else if (memcmp(buff, XDELTA_018_MAGIC, XDELTA_MAGIC_LEN)==0) {
		return 1;
	} else if (memcmp(buff, XDELTA_014_MAGIC, XDELTA_MAGIC_LEN)==0) {
		return 1;
	}
	return 0;
}

unsigned long inline 
readXDInt(cfile *patchf, unsigned char *buff)
{
	unsigned long num=0;
	signed int count=-1;
	do {
		count++;
		cread(patchf, buff + count, 1);
	} while(buff[count] & 0x80);
	for(; count >= 0; count--) {
		num <<= 7;
		num |= (buff[count] & 0x7f); 
	}
	return num;
}

signed int 
xdelta1EncodeDCBuffer(CommandBuffer *buffer, unsigned int version, 
	cfile *out_cfh)
{
	return -1;
}

signed int 
xdelta1ReconstructDCBuff(DCB_SRC_ID src_id, cfile *patchf, CommandBuffer *dcbuff, 
	unsigned int version)
{
	cfile *add_cfh, *ctrl_cfh;
	unsigned long control_offset, control_end, flags;
	unsigned long len, offset, x, count, proc_count;
	unsigned long add_start, add_pos;
	unsigned char buff[32];
	EDCB_SRC_ID ref_id, add_id;
	unsigned char add_is_sequential, copy_is_sequential;
	cseek(patchf, XDELTA_MAGIC_LEN, CSEEK_FSTART);
	cread(patchf, buff, 4);
	flags = readUBytesBE(buff, 4);
	cread(patchf, buff, 4);

	dcbuff->ver_size = 0;
	
	// the header is 32 bytes, then 2 word's, each the length of the 
	// src/trg file name.
	add_start = 32 + readUBytesBE(buff, 2) + readUBytesBE(buff + 2, 2);
	cseek(patchf, -12, CSEEK_END);
	control_end = ctell(patchf, CSEEK_FSTART);
	cread(patchf, buff, 4);
	control_offset = readUBytesBE(buff,4);

	cseek(patchf, control_offset, CSEEK_FSTART);

	if(flags & XD_COMPRESSED_FLAG) {
		v2printf("compressed segments detected\n");
		if((ctrl_cfh = (cfile *)malloc(sizeof(cfile)))==NULL) {
			return MEM_ERROR;
		}
		copen_child_cfh(ctrl_cfh, patchf, control_offset, control_end, 
			GZIP_COMPRESSOR, CFILE_RONLY);
	} else {
		ctrl_cfh = patchf;
	}

	/* kludge. skipping 8 byte unknown, and to_file md5.*/
	cseek(ctrl_cfh, 24, CSEEK_CUR);

	/* read the frigging to length, since it's variable */
	x = readXDInt(ctrl_cfh, buff);
		v2printf("to_len(%lu)\n", x);

	/* two bytes here I don't know about... */
	cseek(ctrl_cfh, 2, CSEEK_CUR);
	/* get and skip the segment name's len and md5 */
	x = readXDInt(ctrl_cfh, buff);

	cseek(ctrl_cfh, x + 16, CSEEK_CUR);

	/* read the damned segment patch len. */
	x = readXDInt(ctrl_cfh, buff);

	/* skip the seq/has data bytes */
	/* handle sequential/has_data info */
	cread(ctrl_cfh, buff, 2);
	add_is_sequential = buff[1];

	v2printf("patch sequential? (%u)\n", add_is_sequential);

	/* get and skip the next segment name len and md5. */
	x = readXDInt(ctrl_cfh, buff);
	cseek(ctrl_cfh, x + 16, CSEEK_CUR);

	/* read the damned segment patch len. */
	x = readXDInt(ctrl_cfh, buff);
	v2printf("seg2_len(%lu)\n", x);

	/* handle sequential/has_data */
	cread(ctrl_cfh, buff, 2);
	copy_is_sequential = buff[1];
	v2printf("copy is sequential? (%u)\n", copy_is_sequential);
	/* next get the number of instructions (eg copy | adds) */
	count = readXDInt(ctrl_cfh, buff);
	proc_count=0;
	/* so starts the commands... */
	v2printf("supposedly %lu commands...\nstarting command processing at %zi\n",
		count, ctell(ctrl_cfh, CSEEK_FSTART));
	if(flags & XD_COMPRESSED_FLAG) {
		add_pos = 0;
		if((add_cfh = (cfile *)malloc(sizeof(cfile)))==NULL) {
			return MEM_ERROR;
		}
		copen_child_cfh(add_cfh, patchf, add_start, control_offset, 
			GZIP_COMPRESSOR, CFILE_RONLY);
		add_id = DCB_REGISTER_ADD_SRC(dcbuff, add_cfh, NULL, 1);
	} else {
		add_pos = add_start;
		add_id = DCB_REGISTER_VOLATILE_ADD_SRC(dcbuff, patchf, NULL, 0);
	}
	ref_id = src_id;
	while(proc_count++ != count) {
		x = readXDInt(ctrl_cfh, buff);
		offset = readXDInt(ctrl_cfh, buff);
		len = readXDInt(ctrl_cfh, buff);
		if(x==XD_INDEX_COPY) {
			DCB_add_copy(dcbuff, offset, 0, len, ref_id);
		} else {
			if(add_is_sequential != 0) {
				offset += add_pos; 
				add_pos += len;
			} else {
				offset += add_pos;
			}
			DCB_add_add(dcbuff, offset, len, add_id);
		}
	}
	v2printf("finishing position was %zi\n", ctell(ctrl_cfh, CSEEK_FSTART));
	v2printf("processed %lu of %lu commands\n", proc_count, count);
	dcbuff->ver_size = dcbuff->reconstruct_pos;
	if(flags & XD_COMPRESSED_FLAG) {
		cclose(ctrl_cfh);
		free(ctrl_cfh);
	}
	return 0;
}


