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
#include <diffball/gdiff.h>
#include <diffball/bit-functions.h>

unsigned int
check_gdiff4_magic(cfile *patchf)
{
	unsigned char buff[GDIFF_MAGIC_LEN + 1];
	cseek(patchf, 0, CSEEK_FSTART);
	if(GDIFF_MAGIC_LEN != cread(patchf, buff, GDIFF_MAGIC_LEN)) {
		return 0;
	} else if(readUBytesBE(buff, GDIFF_MAGIC_LEN)!=GDIFF_MAGIC) {
		return 0;
	} else if (GDIFF_VER_LEN!=cread(patchf, buff, GDIFF_VER_LEN)) {
		return 0;
	} else if(readUBytesBE(buff, GDIFF_VER_LEN) == GDIFF_VER4_MAGIC) {
		return 2;
	}
	return 0;
}

unsigned int
check_gdiff5_magic(cfile *patchf)
{
	unsigned char buff[GDIFF_MAGIC_LEN + 1];
	cseek(patchf, 0, CSEEK_FSTART);
	if(GDIFF_MAGIC_LEN != cread(patchf, buff, GDIFF_MAGIC_LEN)) {
		return 0;
	} else if(readUBytesBE(buff, GDIFF_MAGIC_LEN)!=GDIFF_MAGIC) {
		return 0;
	} else if (GDIFF_VER_LEN!=cread(patchf, buff, GDIFF_VER_LEN)) {
		return 0;
	} else if(readUBytesBE(buff, GDIFF_VER_LEN) == GDIFF_VER5_MAGIC) {
		return 2;
	}
	return 0;
}

signed int 
gdiffEncodeDCBuffer(CommandBuffer *buffer, 
	unsigned int offset_type, cfile *out_cfh)
{
	unsigned char /* *ptr,*/ clen;
	unsigned long fh_pos=0;
	signed long s_off=0;
	unsigned long u_off=0;
	off_u32 delta_pos=0, dc_pos=0;
	unsigned int lb=0, ob=0;
	unsigned char off_is_sbytes=0;
	unsigned char out_buff[5];
	unsigned long count;
	DCommand dc;

	if(offset_type==ENCODING_OFFSET_DC_POS) {
		off_is_sbytes=1;
	} else {
		off_is_sbytes=0;
	}
	writeUBytesBE(out_buff, GDIFF_MAGIC, GDIFF_MAGIC_LEN);
	cwrite(out_cfh, out_buff, GDIFF_MAGIC_LEN);
	if(offset_type==ENCODING_OFFSET_START)
		writeUBytesBE(out_buff, GDIFF_VER4_MAGIC, GDIFF_VER_LEN);
	else if(offset_type==ENCODING_OFFSET_DC_POS)
		writeUBytesBE(out_buff, GDIFF_VER5_MAGIC, GDIFF_VER_LEN);
	else {
		return PATCH_CORRUPT_ERROR;
	}
	cwrite(out_cfh, out_buff, GDIFF_VER_LEN);
	DCBufferReset(buffer);
	count=0;
	while(DCB_commands_remain(buffer)) {
		DCB_get_next_command(buffer, &dc);
		if(dc.data.len==0) {
			DCBufferIncr(buffer);
			continue;
		}
		if(dc.type == DC_ADD) {
			v2printf("add command, delta_pos(%u), src_off(%llu), ver_pos(%llu), len(%llu)\n",
				delta_pos, (act_off_u64)dc.data.src_pos, (act_off_u64)dc.data.ver_pos, (act_off_u64)dc.data.len);
			u_off = dc.data.len;
			if(dc.data.len <= 246) {
				out_buff[0] = dc.data.len;
				cwrite(out_cfh, out_buff, 1);
				delta_pos+=1;
			} else if (dc.data.len <= 0xffff) {
				out_buff[0] = 247;
				writeUBytesBE(out_buff + 1, dc.data.len, 2);
				cwrite(out_cfh, out_buff, 3);
				delta_pos+=3;
			} else if (dc.data.len <= 0xffffffff) {
				out_buff[0] = 248;
				writeUBytesBE(out_buff + 1, dc.data.len, 4);
				cwrite(out_cfh, out_buff, 5);
				delta_pos+=5;
			} else {
				return FORMAT_ERROR;
			}
			if(dc.data.len != copyDCB_add_src(buffer, &dc, out_cfh)) {
				return EOF_ERROR;
			}

			delta_pos += dc.data.len;
			fh_pos += dc.data.len;
		} else {
			if(off_is_sbytes) {
				s_off = (signed long)dc.data.src_pos - (signed long)dc_pos;
				u_off = abs(s_off);
				ob=signedBytesNeeded(s_off);
			} else {
				u_off = dc.data.src_pos;
				ob=unsignedBytesNeeded(u_off);
			}
			lb=unsignedBytesNeeded(dc.data.len);
			if(lb> INT_BYTE_COUNT) {
				return FORMAT_ERROR;
			}
			if(ob > LONG_BYTE_COUNT) {
				return FORMAT_ERROR;
			}
			clen=1;
			if(lb <= BYTE_BYTE_COUNT)
				lb=BYTE_BYTE_COUNT;
			else if(lb <= SHORT_BYTE_COUNT)
				lb=SHORT_BYTE_COUNT;
			else
				lb=INT_BYTE_COUNT;
			if(ob<=SHORT_BYTE_COUNT) {
				ob=SHORT_BYTE_COUNT;
				if(lb == BYTE_BYTE_COUNT)
					out_buff[0]=249;
				else if(lb == SHORT_BYTE_COUNT)
					out_buff[0]=250;
				else
					out_buff[0]=251;
			} else if (ob<=INT_BYTE_COUNT) {
				ob=INT_BYTE_COUNT;
				if(lb == BYTE_BYTE_COUNT)
					out_buff[0]=252;
				else if(lb == SHORT_BYTE_COUNT)
					out_buff[0]=253;
				else
					out_buff[0]=254;
			} else {
				ob=LONG_BYTE_COUNT;
				out_buff[0]=255;
			}
			if(off_is_sbytes) {
				writeSBytesBE(out_buff + clen, s_off, ob);
			} else {
				writeUBytesBE(out_buff + clen, u_off, ob);
			}
			clen+= ob;
			writeUBytesBE(out_buff + clen, dc.data.len, lb);
			clen+=lb;
			v2printf("copy delta_pos(%u), type(%u), src_pos(%llu), ver_pos(%llu), len(%llu)\n",
				delta_pos, out_buff[0], (act_off_u64)dc.data.src_pos, (act_off_u64)dc.data.ver_pos,
				//(off_is_sbytes ?  dc_pos + s_off : u_off), 
				(act_off_u64)dc.data.len);
			if(cwrite(out_cfh, out_buff, clen)!=clen) {
				return IO_ERROR;
			}
			fh_pos += dc.data.len;
			delta_pos+=1 + ob + lb;
			dc_pos += s_off;
		}
	}
	out_buff[0] = 0;
	cwrite(out_cfh, out_buff, 1);
	//ahem.  better error handling/returning needed. in time, in time...
	return 0;
}

signed int 
gdiffReconstructDCBuff(DCB_SRC_ID  src_id, cfile *patchf, CommandBuffer *dcbuff, 
		unsigned int offset_type)
{
	const unsigned int buff_size = 5;
	unsigned char buff[buff_size];
	off_u32 len, dc_pos=0;
	off_u64 ver_pos=0;
	off_u64 u_off=0;
	off_s64 s_off=0;
	EDCB_SRC_ID add_id, ref_id;
	int off_is_sbytes, ob=0, lb=0;

	dcbuff->ver_size = 0;
	if(offset_type==ENCODING_OFFSET_DC_POS)
		off_is_sbytes=1;
	else
		off_is_sbytes=0;
//	assert(DCBUFFER_FULL_TYPE == dcbuff->DCBtype);
	cseek(patchf, 5, CSEEK_CUR);

	add_id = DCB_REGISTER_VOLATILE_ADD_SRC(dcbuff, patchf, NULL, 0);
//	ref_id = DCB_REGISTER_COPY_SRC(dcbuff, ref_cfh, NULL, 0);
	ref_id = src_id;
	while(cread(patchf, buff, 1)==1 && *buff != 0) {
		if(*buff > 0 && *buff <= 248) {
			//add command
			v2printf("add command type(%u) ", *buff);
			if(*buff >=247 && *buff <= 248){
				if (*buff==247)
					lb=2;
				else if (*buff==248)
					lb=4;
				if(cread(patchf, buff, lb)!=lb) {
					return EOF_ERROR;
				}
				len= readUBytesBE(buff, lb);
			} else
				len=*buff;
			DCB_add_add(dcbuff, ctell(patchf, CSEEK_FSTART), len, add_id);
			v2printf("len(%u)\n", len);
			cseek(patchf, len, CSEEK_CUR);
		} else if(*buff >= 249 ) {
			//copy command
			v2printf("copy command ccom(%u) ", *buff);
			if(*buff >=249 && *buff <= 251) {
				ob=2;
				if(*buff==249)
					lb=1;
				else if(*buff==250)
					lb=2;
				else if(*buff==251)
					lb=4;
				} else if (*buff >=252 && *buff <=254) {
					ob=4;
				if(*buff==252)
					lb=1;
				else if(*buff==253)
					lb=2;
				else 
					lb=4;
					} else {
					ob=8;
					lb=4;
					}
				if(cread(patchf, buff + 1, lb + ob)!= lb + ob) {
					return EOF_ERROR;
				}
				if(off_is_sbytes) {
					s_off=readSBytesBE(buff + 1, ob);
					v2printf("s_off=%lld, computed_pos(%llu)\n", (act_off_s64)s_off,
						(act_off_u64)(dc_pos + s_off));
				} else {
					u_off=readUBytesBE(buff + 1, ob);
				}
				len = readUBytesBE(buff + 1 + ob, lb);
				if(off_is_sbytes) {
					dc_pos = u_off = dc_pos + s_off;
				}
				v2printf("offset(%llu), len(%u)\n", (act_off_u64)u_off, len);
				DCB_add_copy(dcbuff, u_off, 0, len, ref_id);
				ver_pos+=len;
			}
	}
	dcbuff->ver_size = dcbuff->reconstruct_pos;
	v2printf("closing command was (%u)\n", *buff);
	v2printf("cread fh_pos(%lu)\n", ctell(patchf, CSEEK_ABS)); 
	v2printf("ver_pos(%llu)\n", (act_off_u64)dcbuff->ver_size);
	return 0;
}
