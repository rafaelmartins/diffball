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
#include <diffball/dcbuffer.h>
#include <diffball/defs.h>
#include <cfile.h>
#include <diffball/bit-functions.h>
#include <diffball/bdelta.h>

unsigned int
check_bdelta_magic(cfile *patchf)
{
	unsigned char buff[BDELTA_MAGIC_LEN + 1];
	cseek(patchf, 0, CSEEK_FSTART);
	if(BDELTA_MAGIC_LEN != cread(patchf, buff, BDELTA_MAGIC_LEN)) {
		return 0;
	} else if (memcmp(buff, BDELTA_MAGIC, BDELTA_MAGIC_LEN)!=0) {
		return 0;
	}
	return 2;
}

signed int 
bdeltaEncodeDCBuffer(CommandBuffer *dcbuff, cfile *patchf)
{
	off_u32  dc_pos, total_count, count, maxnum, matches;
	off_u32  add_len, copy_len;
	off_u64  copy_offset;
	unsigned char prev, current;
	unsigned int intsize;
	unsigned char buff[16];
	off_u32  match_orig;
	unsigned long ver_size = 0;
	DCommand dc;

	cwrite(patchf, BDELTA_MAGIC, BDELTA_MAGIC_LEN);
	writeUBytesLE(buff, BDELTA_VERSION, BDELTA_VERSION_LEN);
	cwrite(patchf, buff, BDELTA_VERSION_LEN);
	total_count = 0;

	DCBufferReset(dcbuff);
	/* since this will be collapsing all adds... */
	current = DC_COPY;
	matches=0;
	while(DCB_commands_remain(dcbuff)) {
		total_count++;
		DCB_get_next_command(dcbuff, &dc);
		prev = current;
		current = dc.type;
		ver_size += dc.data.len;
		if(! ((prev==DC_ADD && current==DC_COPY) ||
			(prev==DC_ADD && current==DC_ADD)) )
			matches++;
	}
	maxnum = MAX(matches, MAX(dcbuff->src_size, dcbuff->ver_size) );
/*	if(maxnum <= 0x7f)
		intsize=1;
	else if(maxnum <= 0x7fff)
		intsize=2;
	else if(maxnum <= 0x7fffff)
		intsize=3;
	else*/
		intsize=4;
	
	v2printf("size1=%llu, size2=%llu, matches=%u, intsize=%u\n", (act_off_u64)dcbuff->src_size, 
		(act_off_u64)(dcbuff->ver_size ? dcbuff->ver_size : ver_size), matches, intsize);

	buff[0] = intsize;
	cwrite(patchf, buff, 1);
	writeUBytesLE(buff, dcbuff->src_size, intsize);
	writeUBytesLE(buff + intsize, (dcbuff->ver_size ? dcbuff->ver_size : 
ver_size), intsize);
	writeUBytesLE(buff + (2 * intsize), matches, intsize);
	cwrite(patchf, buff, (3 * intsize));
	DCBufferReset(dcbuff);
	dc_pos=0;
	match_orig = matches;
	count = total_count;
	while(matches--) {
		DCB_get_next_command(dcbuff, &dc);
		v2printf("handling match(%u)\n", match_orig - matches);
		add_len=0;
		if(DC_ADD==dc.type) {
			do {
				add_len += dc.data.len;
					count--;
				DCB_get_next_command(dcbuff, &dc);
			} while(count!=0 && DC_ADD == dc.type);
			v2printf("writing add len=%u\n", add_len);
		}
		/* basically a fall through to copy, if count!=0 */
		if(count != 0) {
			copy_len = dc.data.len;
//			if(dc_pos > dc.loc.offset) {
			if(dc_pos > dc.data.src_pos) {
				v2printf("negative offset, dc_pos(%u), offset(%llu)\n",
					dc_pos, (act_off_u64)dc.data.src_pos);
				copy_offset = dc.data.src_pos + (~dc_pos + 1);
			} else {
				v2printf("positive offset, dc_pos(%u), offset(%llu)\n",
					dc_pos, (act_off_u64)dc.data.src_pos);
				copy_offset = dc.data.src_pos - dc_pos;
			}
			dc_pos = dc.data.src_pos + dc.data.len;
			v2printf("writing copy_len=%u, offset=%llu, dc_pos=%u\n",
				copy_len, (act_off_u64)dc.data.src_pos, dc_pos);
		} else {
			copy_len = 0;
			copy_offset = 0;
		}
		writeUBytesLE(buff, copy_offset, intsize);
		writeUBytesLE(buff + intsize, add_len, intsize);
		writeUBytesLE(buff + (2 * intsize), copy_len, intsize);
		cwrite(patchf, buff, (3 * intsize));
		count--;
	}
	/* control block wrote. */
	DCBufferReset(dcbuff);
	v2printf("writing add_block at %u\n", (off_u32)ctell(patchf, CSEEK_FSTART));
	while(DCB_commands_remain(dcbuff)) {
		DCB_get_next_command(dcbuff, &dc);
		if(DC_ADD == dc.type) {
			if(dc.data.len != copyDCB_add_src(dcbuff, &dc, patchf)) {
				return EOF_ERROR;
			}
		}
	}
	return 0;
}

signed int 
bdeltaReconstructDCBuff(DCB_SRC_ID src_id, cfile *patchf, CommandBuffer *dcbuff)
{
	unsigned int int_size;
	#define BUFF_SIZE 12
	unsigned int ver;
	EDCB_SRC_ID ref_id, add_id;
	unsigned char buff[BUFF_SIZE];
	off_s64  copy_offset;
	off_u32  match_orig, matches, add_len, copy_len;
	off_u32  size1, size2, or_mask=0, neg_mask;
	off_u64  ver_pos = 0, add_pos;
	off_u64  processed_size = 0;
	off_u32  add_start;

	dcbuff->ver_size = 0;
//	assert(DCBUFFER_FULL_TYPE == dcbuff->DCBtype);
	if(3!=cseek(patchf, 3, CSEEK_FSTART))
		goto truncated_patch;
	if(2!=cread(patchf, buff, BDELTA_VERSION_LEN))
		goto truncated_patch;
	ver = readUBytesLE(buff, 2);
	v2printf("ver=%u\n", ver);
	cread(patchf, buff, 1);
	int_size = buff[0];
	v2printf("int_size=%u\n", int_size);
	if(int_size < 1 || int_size > 4)
		return PATCH_CORRUPT_ERROR;
	/* yes, this is an intentional switch fall through. */
	switch(int_size) {
		case 1: or_mask |= 0x0000ff00;
		case 2: or_mask |= 0x00ff0000;
		case 3: or_mask |= 0xff000000; 
	}
	/* yes this will have problems w/ int_size==0 */
	neg_mask = (1 << ((int_size * 8) -1));
	cread(patchf, buff, 3 * int_size);
	size1 = readUBytesLE(buff, int_size);
	size2 = readUBytesLE(buff + int_size, int_size);
	v1printf("size1=%u, size2=%u\n", size1, size2);
	dcbuff->src_size = size1;
	dcbuff->ver_size = size2;
	matches = readUBytesLE(buff + (2 * int_size), int_size);
	v2printf("size1=%u, size2=%u\nmatches=%u\n", size1, size2, matches);
	/* add_pos = header info, 3 int_size's (size(1|2), num_match) */
	add_pos = 3 + 2 + 1 + (3 * int_size);
	/* add block starts after control data. */
	add_pos += (matches * (3 * int_size));
	add_start = add_pos;
	add_id = DCB_REGISTER_VOLATILE_ADD_SRC(dcbuff, patchf, NULL, 0);
//	ref_id = DCB_REGISTER_ADD_SRC(dcbuff, ref_cfh, NULL, 0);
	ref_id = src_id;
	v2printf("add block starts at %u\nprocessing commands\n", add_pos);
	match_orig = matches;
	if(size1==0) {
		v0printf("size1 was zero, processing anyways.\n");
		v0printf("this patch should be incompatible w/ bdelta,\n");
		v0printf("although I have no problems reading it.\n");
	}
	while(matches--){
		v2printf("handling match(%u)\n", match_orig - matches);
		cread(patchf, buff, 3 * int_size);
		copy_offset = readUBytesLE(buff, int_size);
		add_len = readUBytesLE(buff + int_size, int_size);
		copy_len = readUBytesLE(buff + int_size * 2, int_size);
			if(add_len) {
			v2printf("add  len(%u)\n", add_len);
			DCB_add_add(dcbuff, add_pos, add_len, add_id);
			add_pos += add_len;
		}
		/* hokay, this is screwed up.  Course bdelta seems like it's got 
		a possible problem w/ files produced on one's complement systems when
		read on a 2's complement system. */
		if((copy_offset & neg_mask) > 0) {
			copy_offset |= or_mask;
			ver_pos -= (~copy_offset) +1;
			v2printf("ver_pos now(%llu)\n", (act_off_u64)ver_pos);
		} else {
			v2printf("positive offset(%llu)\n", (act_off_u64)copy_offset);
			ver_pos += copy_offset;
		}
		/* an attempt to ensure things aren't whacky. */
		assert(size1==0 || ver_pos <= size1);
		if(copy_len) {
			v2printf("copy len(%u), off(%lld), pos(%llu)\n", 
				copy_len, (act_off_s64)copy_offset, (act_off_u64)ver_pos);
			DCB_add_copy(dcbuff, ver_pos, 0, copy_len, ref_id);
			ver_pos += copy_len;
		}
		processed_size += add_len + copy_len;
	}
	assert(ctell(patchf, CSEEK_FSTART)==add_start);
	if(processed_size != size2) {
		v1printf("hmm, left the trailing nulls out; adding appropriate command\n");
		DCB_add_add(dcbuff, add_pos, size2 - processed_size, 0);
	}
	dcbuff->ver_size = dcbuff->reconstruct_pos;
	v2printf("finished reading.  ver_pos=%llu, add_pos=%u\n",
		(act_off_u64)ver_pos, add_pos);
	return 0;

	truncated_patch:
	return PATCH_TRUNCATED;
}

