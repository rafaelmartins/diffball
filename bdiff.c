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
#include "bdiff.h"
#include "bit-functions.h"

unsigned int
check_bdiff_magic(cfile *patchf)
{
    unsigned char buff[BDIFF_MAGIC_LEN + 1];
    cseek(patchf, 0, CSEEK_FSTART);
    if(BDIFF_MAGIC_LEN + 1 != cread(patchf, buff, BDIFF_MAGIC_LEN + 1)) {
	return 0;
    }
    if(memcmp(buff, BDIFF_MAGIC, BDIFF_MAGIC_LEN)!=0) {
	return 0;
    }
    if(buff[BDIFF_MAGIC_LEN]==BDIFF_VERSION) {
	return 2;
    }
    return 1;
}

signed int 
bdiffEncodeDCBuffer(CommandBuffer *buffer, cfile *out_cfh)
{
#define BUFFER_SIZE 1024
//    unsigned char src_md5[16], ver_md5[16];
    unsigned char buff[BUFFER_SIZE];
    unsigned long count, fh_pos, delta_pos;
    unsigned int lb;
    DCommand dc;
    DCBufferReset(buffer);
    memcpy(buff, BDIFF_MAGIC, BDIFF_MAGIC_LEN);
    buff[BDIFF_MAGIC_LEN] = BDIFF_VERSION;
    /* I haven't studied the author of bdiff's alg well enough too know what
    MaxBlockSize is for.  Either way, throwing in the default. */
    writeUBytesBE(buff + BDIFF_MAGIC_LEN + 1,
	(BDIFF_DEFAULT_MAXBLOCKSIZE), 4);
    cwrite(out_cfh, buff, BDIFF_MAGIC_LEN + 5);
    delta_pos = 10;
    fh_pos = 0;
    count=0;
    while(DCB_commands_remain(buffer)) {
//    while(count--) {
	DCB_get_next_command(buffer, &dc);
	if(DC_COPY == dc.type) {
	    v2printf("copy command, out_cfh(%lu), fh_pos(%lu), offset(%lu), len(%lu)\n",
		delta_pos, fh_pos, dc.data.src_pos, dc.data.len);
	    fh_pos += dc.data.len;
	    lb = 5;
	    buff[0] = 0;
	    writeUBytesBE(buff + 1, dc.data.src_pos, 4);
	    if(dc.data.len > 5 && dc.data.len <= 5 + 0x3f) {
		buff[0] = dc.data.len -5 ;
	    } else {
		writeUBytesBE(buff + 5, dc.data.len, 4);
		lb += 4;
	    }
	    delta_pos += lb;
	    cwrite(out_cfh, buff, lb);
	} else {
	    v2printf("add  command, out_cfh(%lu), fh_pos(%lu), len(%lu)\n", 
		delta_pos, fh_pos, dc.data.len);
	    fh_pos += dc.data.len;
	    buff[0] = 0x80;
	    lb = 1;
	    if(dc.data.len > 5 && dc.data.len <= 5 + 0x3f) {
		buff[0] |= dc.data.len - 5;
	    } else {
		writeUBytesBE(buff + 1, dc.data.len, 4);
		lb += 4;
	    }
	    delta_pos += lb + dc.data.len;
	    cwrite(out_cfh, buff, lb);
	    if(dc.data.len != copyDCB_add_src(buffer, &dc, out_cfh)) {
		return EOF_ERROR;
	    }
	}
    }
    return 0;
}

signed int 
//bdiffReconstructDCBuff(cfile *ref_cfh, cfile *patchf, CommandBuffer *dcbuff)
bdiffReconstructDCBuff(unsigned char src_id, cfile *patchf, CommandBuffer *dcbuff)
{
    unsigned char src_md5[16], ver_md5[16], buff[17];
    unsigned long len, offset, maxlength;
    unsigned long fh_pos;
    unsigned char ref_id, add_id;

    dcbuff->ver_size = 0;
    memset(src_md5, 0, 16);
    memset(ver_md5, 0, 16);
    /* skippping magic bdiff, and version char 'a' */
    cseek(patchf, 6, CSEEK_ABS);
    cread(patchf, buff, 4);
    /* what the heck is maxlength used for? */
    maxlength = readUBytesBE(buff, 4);
    fh_pos = 0;
    add_id = DCB_REGISTER_VOLATILE_ADD_SRC(dcbuff, patchf, NULL, 0);
//    ref_id = DCB_REGISTER_COPY_SRC(dcbuff, ref_cfh, NULL, 0);
    ref_id = src_id;
    while(1 == cread(patchf, buff, 1)) {
	v2printf("got command(%u): ", buff[0]);
	if((buff[0] >> 6)==00) {
	    buff[0] &= 0x3f;
	    v2printf("got a copy at %lu, fh_pos(%lu): ", 
		ctell(patchf, CSEEK_FSTART), fh_pos);
	    if(4 != cread(patchf, buff + 1, 4)) {
		return EOF_ERROR;
	    }
	    offset = readUBytesBE(buff + 1, 4);
	    if(buff[0]) {
		len = readUBytesBE(buff, 1) + 5;
	    } else {
		if(4 != cread(patchf, buff, 4)) {
		    return EOF_ERROR;
		}
		len = readUBytesBE(buff, 4);
	    }
	    fh_pos += len;
	    v2printf(" offset(%lu), len=%lu\n", offset, len);
	    DCB_add_copy(dcbuff, offset, 0, len, ref_id);
	} else if ((buff[0] >> 6)==2) {
	    buff[0] &= 0x3f;
	    v2printf("got an add at %lu, fh_pos(%lu):", 
		ctell(patchf, CSEEK_FSTART), fh_pos);
	    if(buff[0]) {
		len = readUBytesBE(buff, 1) + 5;
	    } else {
		if(4 != cread(patchf, buff, 4)) {
		    return EOF_ERROR;
		}
		len = readUBytesBE(buff, 4);
	    }
	    fh_pos += len;
	    v2printf(" len=%lu\n", len);
	    DCB_add_add(dcbuff, ctell(patchf, CSEEK_FSTART), len, add_id);
	    cseek(patchf, len, CSEEK_CUR);
	} else if((buff[0] >> 6)==1) {
	    buff[0] &= 0x3f;
	    v2printf("got a checksum at %lu\n", ctell(patchf, CSEEK_FSTART));
	    if(buff[0] <= 1) {
		if(16 != cread(patchf, buff + 1, 16)) 
		    return EOF_ERROR;
		if(buff[0]==0) 
		    memcpy(src_md5, buff + 1, 16);
		else 
		    memcpy(ver_md5, buff + 1, 16);
	    } else {
		return EOF_ERROR;
	    }
	}
    }
    dcbuff->ver_size = dcbuff->reconstruct_pos;
    return 0;
}
