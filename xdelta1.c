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
#include "cfile.h"
#include "defs.h"
#include "bit-functions.h"
#include "xdelta1.h"

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
    cfile *ver_cfh, cfile *out_cfh)
{
    return 0;
}

signed int 
xdelta1ReconstructDCBuff(cfile *patchf, CommandBuffer *dcbuff, 
    unsigned int version)
{

    unsigned long control_offset, add_start, flags;
    unsigned long len, offset, x, count;
    unsigned long add_pos;
    unsigned char buff[32];
    unsigned char add_is_sequential, copy_is_sequential;
    cseek(patchf, 8, CSEEK_FSTART);
    cread(patchf, buff, 4);
    flags = readUBytesBE(buff, 4);
    cread(patchf, buff, 4);
    add_start = 32 + readUBytesBE(buff, 2) + 
	readUBytesBE(buff + 2, 2);
    cseek(patchf, -12, CSEEK_END);
    cread(patchf, buff, 4);
    control_offset = readUBytesBE(buff,4);
    cseek(patchf, control_offset, CSEEK_FSTART);
    /* kludge. skipping 8 byte unknown, and to_file md5.*/
    cseek(patchf, 24, CSEEK_CUR);
    /* read the frigging to length, since it's variable */
    x = readXDInt(patchf, buff);
	v2printf("to_len(%lu)\n", x);
    /* two bytes here I don't know about... */
    cseek(patchf, 2, CSEEK_CUR);
    /* get and skip the segment name's len and md5 */
    x = readXDInt(patchf, buff);
	//v2printf("seg1_len(%lu)\n", x);
    cseek(patchf, x + 16, CSEEK_CUR);
    /* read the damned segment patch len. */
    x = readXDInt(patchf, buff);
    /* skip the seq/has data bytes */
    /* handle sequential/has_data info */
    cread(patchf, buff, 2);
    add_is_sequential = buff[1];
    v2printf("patch sequential? (%u)\n", add_is_sequential);
    /* get and skip the next segment name len and md5. */
    x = readXDInt(patchf, buff);
	//v2printf("seg2_len(%lu)\n", x);
    cseek(patchf, x + 16, CSEEK_CUR);
    /* read the damned segment patch len. */
    x = readXDInt(patchf, buff);
	v2printf("seg2_len(%lu)\n", x);
    /* handle sequential/has_data */
    cread(patchf, buff, 2);
    copy_is_sequential = buff[1];
    v2printf("copy is sequential? (%u)\n", copy_is_sequential);
    /* next get the number of instructions (eg copy | adds) */
    count = readXDInt(patchf, buff);
    /* so starts the commands... */
    v2printf("supposedly %lu commands...\nstarting command processing at %lu\n", 
	count, ctell(patchf, CSEEK_FSTART));
    add_pos = add_start;
    while(count--) {
	x = readXDInt(patchf, buff);
	offset = readXDInt(patchf, buff);
	len = readXDInt(patchf, buff);
	if(x==XD_INDEX_COPY) {
	    DCBufferAddCmd(dcbuff, DC_COPY, offset, len);
	} else {
	    if(add_is_sequential != 0) {
		offset += add_pos; 
		add_pos += len;
	    } else {
		offset += add_start;
	    }
	    DCBufferAddCmd(dcbuff, DC_ADD, offset, len);
	}
    }
    v2printf("finishing position was %lu\n", ctell(patchf, CSEEK_FSTART));
    return 0;
}


