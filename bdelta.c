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
#include <assert.h>
#include "dcbuffer.h"
#include "raw.h"
#include "cfile.h"
#include "bit-functions.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

signed int 
bdeltaEncodeDCBuffer(CommandBuffer *buffer, cfile *ver_cfh, 
    struct cfile *patchf)
{
    unsigned char buff[1024];
    unsigned long dc_pos, matches;
    buff[0] = 'B';
    buff[1] = 'D';
    buff[2] = 'T';
    writeUBytesLE(buff + 3, 1, 2); //version
    buff[5] = 4;
    cwrite(patchf, buff, 6);
    
}

signed int 
bdeltaReconstructDCBuff(cfile *patchf, CommandBuffer *dcbuff)
{

    unsigned int int_size;
    #define BUFF_SIZE 12
    unsigned int ver;
    unsigned char buff[BUFF_SIZE];
    unsigned long matches, add_len, copy_len, copy_offset;
    unsigned long size1, size2, or_mask=0;
    unsigned long ver_pos = 0, add_pos;
    unsigned long add_start;
    cseek(patchf, 3, CSEEK_FSTART);
    cread(patchf, buff, 2);
    ver = readUBytesLE(buff, 2);
    printf("ver=%u\n", ver);
    cread(patchf, buff, 1);
    int_size = buff[0];
    printf("int_size=%u\n", int_size);
    /* yes, this is an intentional switch fall through. */
    switch(int_size) {
	case 1: or_mask |= 0x0000ff00;
	case 2: or_mask |= 0x00ff0000;
	case 3: or_mask |= 0xff000000;
    }
    cread(patchf, buff, 3 * int_size);
    size1 = readUBytesLE(buff, int_size);
    size2 = readUBytesLE(buff + int_size, int_size);
    matches = readUBytesLE(buff + (2 * int_size), int_size);
    printf("size1=%lu, size2=%lu\nmatches=%lu\n", size1, size2, matches);
    /* add_pos = header info, 3 int_size's (size(1|2), num_match) */
    add_pos = 3 + 2 + 1 + 3 * int_size;
    /* add block starts after control data. */
    add_pos += (matches * 3 * int_size);
    add_start = add_pos;
    printf("add block starts at %lu\nprocessing commands\n", add_pos);
    while(matches--){
	cread(patchf, buff, 3 * int_size);
	copy_offset = readUBytesLE(buff, int_size);
	add_len = readUBytesLE(buff + int_size, int_size);
	copy_len = readUBytesLE(buff + int_size * 2, int_size);
    	if(add_len) {
	    printf("add  len(%lu)\n", add_len);
	    DCBufferAddCmd(dcbuff, DC_ADD, add_pos, add_len);
	    add_pos += add_len;
	}
	/* hokay, this is screwed up.  Course bdelta seems like it's got 
	a possible problem w/ files produced on one's complement systems when
	read on a 2's complement system. */
	if(buff[0] & 0x80) {
	    copy_offset |= or_mask;
	    copy_offset = ~copy_offset;
	    copy_offset += 1;
	    printf("and it's negative, off(%lu), ver_pos(%lu), ",
		copy_offset, ver_pos);
	    ver_pos -= copy_offset;
	    printf("ver_pos now(%lu)\n", ver_pos);
	} else {
	    printf("positive offset\n", copy_offset);
	    ver_pos += copy_offset;
	}
	/* an attempt to ensure things aren't whacky. */
	assert(ver_pos <= size1);
	if(copy_len) {
	    printf("copy len(%lu), pos(%lu)\n", copy_len, ver_pos);
	    DCBufferAddCmd(dcbuff, DC_COPY, ver_pos, copy_len);
	    ver_pos += copy_len;
	}
//	ver_pos += add_len;
    }
    assert(ctell(patchf, CSEEK_FSTART)==add_start);
    printf("finished reading.  ver_pos=%lu, add_pos=%lu\n",
	ver_pos, add_pos);
    return 0;
}

