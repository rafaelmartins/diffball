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
#include "gdiff.h"
//#include "cfile.h"
#include "bit-functions.h"

signed int bdiffEncodeDCBuffer(struct CommandBuffer *buffer, 
    unsigned int offset_type, 
    struct cfile *ver_cfh, struct cfile *out_fh)
{
    return 0;
}

signed int bdiffReconstructDCBuff(struct cfile *patchf, 
    struct CommandBuffer *dcbuff)
{
    unsigned char src_md5[16], ver_md5[16], buff[17];
    unsigned long len, offset, maxlength;
    memset(src_md5, 0, 16);
    memset(ver_md5, 0, 16);
    /* skippping magic bdiff, and version char 'a' */
    cseek(patchf, 6, CSEEK_ABS);
    cread(patchf, buff, 4);
    /* what the heck is maxlength used for? */
    maxlength = readUnsignedBytes(buff, 4);
    while(1 == cread(patchf, buff, 1)) {
	printf("got command(%u): ", buff[0]);
	if((buff[0] >> 6)==00) {
	    buff[0] &= 0x3f;
	    printf("got a copy at %lu:", ctell(patchf, CSEEK_FSTART));
	    if(4 != cread(patchf, buff + 1, 4)) {
		abort();
	    }
	    offset = readUnsignedBytes(buff + 1, 4);
	    if(buff[0]) {
		len = readUnsignedBytes(buff, 1) + 5;
	    } else {
		if(4 != cread(patchf, buff, 4)) {
		    abort();
		}
		len = readUnsignedBytes(buff, 4);
	    }
	    printf(" len=%lu\n", len);
	    DCBufferAddCmd(dcbuff, DC_COPY, offset, len);
	} else if ((buff[0] >> 6)==2) {
	    buff[0] &= 0x3f;
	    printf("got an add at %lu:", ctell(patchf, CSEEK_FSTART));
	    if(buff[0]) {
		len = readUnsignedBytes(buff, 1) + 5;
	    } else {
		if(4 != cread(patchf, buff, 4)) {
		    abort();
		}
		len = readUnsignedBytes(buff, 4);
	    }
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
