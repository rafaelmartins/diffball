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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "bdiff.h"
//#include "cfile.h"
#include "bit-functions.h"

signed int 
bdiffEncodeDCBuffer(CommandBuffer *buffer, cfile *ver_cfh, cfile *out_cfh)
{
#define BUFFER_SIZE 1024
    unsigned char src_md5[16], ver_md5[16];
    unsigned char buff[BUFFER_SIZE];
    unsigned long count, len, delta_pos, fh_pos;
    unsigned int lb;
    count = buffer->buffer_count;
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    memcpy(buff, BDIFF_MAGIC, BDIFF_LEN_MAGIC);
    buff[BDIFF_LEN_MAGIC] = BDIFF_VERSION;
    /* I haven't studied the author of bdiff's alg well enough too know what
    MaxBlockSize is for.  Either way, throwing in the default. */
//    convertUBytesChar(buff + BDIFF_LEN_MAGIC +1, 
    writeUBytesBE(buff + BDIFF_LEN_MAGIC + 1,
	(BDIFF_DEFAULT_MAXBLOCKSIZE), 4);
    cwrite(out_cfh, buff, BDIFF_LEN_MAGIC + 5);
    delta_pos = 10;
    fh_pos = 0;
    while(count--) {
	if(DC_COPY==get_current_command_type(buffer)) {
	    printf("copy command, out_cfh(%lu), fh_pos(%lu), offset(%lu), len(%lu)\n",
		delta_pos, fh_pos, buffer->lb_tail->offset, 
		buffer->lb_tail->len);
	    fh_pos += buffer->lb_tail->len;
	    lb = 5;
	    buff[0] = 0;
	    writeUBytesBE(buff + 1, buffer->lb_tail->offset, 4);
//	    convertUBytesChar(buff + 1, buffer->lb_tail->offset, 4);
	    if(buffer->lb_tail->len > 5 && 
		buffer->lb_tail->len <= 5 + 0x3f) {
		buff[0] = buffer->lb_tail->len -5;
	    } else {
		writeUBytesBE(buff + 5, buffer->lb_tail->len, 4);
//		convertUBytesChar(buff + 5, buffer->lb_tail->len, 4);
		lb += 4;
	    }
	    delta_pos += lb;
	    cwrite(out_cfh, buff, lb);
	} else {
	    printf("add  command, out_cfh(%lu), fh_pos(%lu), len(%lu)\n", 
		delta_pos, fh_pos, buffer->lb_tail->len);
	    fh_pos += buffer->lb_tail->len;
	    buff[0] = 0x80;
	    lb = 1;
	    if(buffer->lb_tail->len > 5 && 
		buffer->lb_tail->len <= 5 + 0x3f) {
		buff[0] |= buffer->lb_tail->len - 5;
	    } else {
		writeUBytesBE(buff + 1, buffer->lb_tail->len, 4);
//		convertUBytesChar(buff + 1, buffer->lb_tail->len, 4);
		lb += 4;
	    }
	    delta_pos += lb + buffer->lb_tail->len;
	    cwrite(out_cfh, buff, lb);
	    len = buffer->lb_tail->len;
	    if(cseek(ver_cfh, buffer->lb_tail->offset, CSEEK_FSTART) !=
		buffer->lb_tail->offset)
		abort();
	    while(len) {
		lb = MIN(len, BUFFER_SIZE);
		if(cread(ver_cfh, buff, lb)!= lb)
		    abort();
		if(cwrite(out_cfh, buff, lb)!= lb)
		    abort();
		len -= lb;
	    }
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
    unsigned long delta_pos, fh_pos;
    memset(src_md5, 0, 16);
    memset(ver_md5, 0, 16);
    /* skippping magic bdiff, and version char 'a' */
    cseek(patchf, 6, CSEEK_ABS);
    cread(patchf, buff, 4);
    /* what the heck is maxlength used for? */
    maxlength = readUBytesBE(buff, 4);
//    maxlength = readUnsignedBytes(buff, 4);
    fh_pos = 0;
    while(1 == cread(patchf, buff, 1)) {
	printf("got command(%u): ", buff[0]);
	if((buff[0] >> 6)==00) {
	    buff[0] &= 0x3f;
	    printf("got a copy at %lu, fh_pos(%lu): ", 
		ctell(patchf, CSEEK_FSTART), fh_pos);
	    if(4 != cread(patchf, buff + 1, 4)) {
		abort();
	    }
	    offset = readUBytesBE(buff + 1, 4);
//	    offset = readUnsignedBytes(buff + 1, 4);
	    if(buff[0]) {
		len = readUBytesBE(buff, 1) + 5;
//		len = readUnsignedBytes(buff, 1) + 5;
	    } else {
		if(4 != cread(patchf, buff, 4)) {
		    abort();
		}
		len = readUBytesBE(buff, 4);
//		len = readUnsignedBytes(buff, 4);
	    }
	    fh_pos += len;
	    printf(" offset(%lu), len=%lu\n", offset, len);
	    DCBufferAddCmd(dcbuff, DC_COPY, offset, len);
	} else if ((buff[0] >> 6)==2) {
	    buff[0] &= 0x3f;
	    printf("got an add at %lu, fh_pos(%lu):", 
		ctell(patchf, CSEEK_FSTART), fh_pos);
	    if(buff[0]) {
		len = readUBytesBE(buff, 1) + 5;
//		len = readUnsignedBytes(buff, 1) + 5;
	    } else {
		if(4 != cread(patchf, buff, 4)) {
		    abort();
		}
		len = readUBytesBE(buff, 4);
//		len = readUnsignedBytes(buff, 4);
	    }
	    fh_pos += len;
	    printf(" len=%lu\n", len);
	    DCBufferAddCmd(dcbuff, DC_ADD, ctell(patchf, CSEEK_FSTART), len);
	    cseek(patchf, len, CSEEK_CUR);
	} else if((buff[0] >> 6)==1) {
	    buff[0] &= 0x3f;
	    printf("got a checksum at %lu\n", ctell(patchf, CSEEK_FSTART));
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
    return 0;
}
