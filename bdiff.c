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
bdiffEncodeDCBuffer(CommandBuffer *buffer, cfile *ver_cfh, cfile *out_cfh)
{
#define BUFFER_SIZE 1024
//    unsigned char src_md5[16], ver_md5[16];
    unsigned char buff[BUFFER_SIZE];
    unsigned long count, fh_pos, delta_pos;
    unsigned int lb;
    count = DCBufferReset(buffer);
    memcpy(buff, BDIFF_MAGIC, BDIFF_MAGIC_LEN);
    buff[BDIFF_MAGIC_LEN] = BDIFF_VERSION;
    /* I haven't studied the author of bdiff's alg well enough too know what
    MaxBlockSize is for.  Either way, throwing in the default. */
    writeUBytesBE(buff + BDIFF_MAGIC_LEN + 1,
	(BDIFF_DEFAULT_MAXBLOCKSIZE), 4);
    cwrite(out_cfh, buff, BDIFF_MAGIC_LEN + 5);
    delta_pos = 10;
    fh_pos = 0;
    while(count--) {
	if(DC_COPY==current_command_type(buffer)) {
	    v2printf("copy command, out_cfh(%lu), fh_pos(%lu), offset(%lu), len(%lu)\n",
		delta_pos, fh_pos, DCBF_cur_off(buffer), 
		DCBF_cur_len(buffer));
	    fh_pos += DCBF_cur_len(buffer);
	    lb = 5;
	    buff[0] = 0;
	    writeUBytesBE(buff + 1, DCBF_cur_off(buffer), 4);
	    if(DCBF_cur_len(buffer) > 5 && 
		DCBF_cur_len(buffer) <= 5 + 0x3f) {
		buff[0] = DCBF_cur_len(buffer) -5;
	    } else {
		writeUBytesBE(buff + 5, DCBF_cur_len(buffer), 4);
		lb += 4;
	    }
	    delta_pos += lb;
	    cwrite(out_cfh, buff, lb);
	} else {
	    v2printf("add  command, out_cfh(%lu), fh_pos(%lu), len(%lu)\n", 
		delta_pos, fh_pos, DCBF_cur_len(buffer));
	    fh_pos += DCBF_cur_len(buffer);
	    buff[0] = 0x80;
	    lb = 1;
	    if(DCBF_cur_len(buffer) > 5 && 
		DCBF_cur_len(buffer) <= 5 + 0x3f) {
		buff[0] |= DCBF_cur_len(buffer) - 5;
	    } else {
		writeUBytesBE(buff + 1, DCBF_cur_len(buffer), 4);
		lb += 4;
	    }
	    delta_pos += lb + DCBF_cur_len(buffer);
	    cwrite(out_cfh, buff, lb);
	    if(DCBF_cur_len(buffer) != 
		copy_cfile_block(out_cfh, ver_cfh, DCBF_cur_off(buffer), 
		DCBF_cur_len(buffer)))
		abort();
	}
	DCBufferIncr(buffer);
    }
    return 0;
}

signed int 
bdiffReconstructDCBuff(cfile *patchf, CommandBuffer *dcbuff)
{
    unsigned char src_md5[16], ver_md5[16], buff[17];
    unsigned long len, offset, maxlength;
    unsigned long fh_pos;
    memset(src_md5, 0, 16);
    memset(ver_md5, 0, 16);
    /* skippping magic bdiff, and version char 'a' */
    cseek(patchf, 6, CSEEK_ABS);
    cread(patchf, buff, 4);
    /* what the heck is maxlength used for? */
    maxlength = readUBytesBE(buff, 4);
    fh_pos = 0;
    while(1 == cread(patchf, buff, 1)) {
	v2printf("got command(%u): ", buff[0]);
	if((buff[0] >> 6)==00) {
	    buff[0] &= 0x3f;
	    v2printf("got a copy at %lu, fh_pos(%lu): ", 
		ctell(patchf, CSEEK_FSTART), fh_pos);
	    if(4 != cread(patchf, buff + 1, 4)) {
		abort();
	    }
	    offset = readUBytesBE(buff + 1, 4);
	    if(buff[0]) {
		len = readUBytesBE(buff, 1) + 5;
	    } else {
		if(4 != cread(patchf, buff, 4)) {
		    abort();
		}
		len = readUBytesBE(buff, 4);
	    }
	    fh_pos += len;
	    v2printf(" offset(%lu), len=%lu\n", offset, len);
	    DCBufferAddCmd(dcbuff, DC_COPY, offset, len);
	} else if ((buff[0] >> 6)==2) {
	    buff[0] &= 0x3f;
	    v2printf("got an add at %lu, fh_pos(%lu):", 
		ctell(patchf, CSEEK_FSTART), fh_pos);
	    if(buff[0]) {
		len = readUBytesBE(buff, 1) + 5;
	    } else {
		if(4 != cread(patchf, buff, 4)) {
		    abort();
		}
		len = readUBytesBE(buff, 4);
	    }
	    fh_pos += len;
	    v2printf(" len=%lu\n", len);
	    DCBufferAddCmd(dcbuff, DC_ADD, ctell(patchf, CSEEK_FSTART), len);
	    cseek(patchf, len, CSEEK_CUR);
	} else if((buff[0] >> 6)==1) {
	    buff[0] &= 0x3f;
	    v2printf("got a checksum at %lu\n", ctell(patchf, CSEEK_FSTART));
	    if(buff[0] <= 1) {
		if(16 != cread(patchf, buff + 1, 16)) 
		    abort();
		if(buff[0]==0) 
		    memcpy(src_md5, buff + 1, 16);
		else 
		    memcpy(ver_md5, buff + 1, 16);
	    } else {
		abort();
	    }
	}
    }
    DCBUFFER_REGISTER_ADD_CFH(dcbuff, patchf);
    return 0;
}
