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
#ifndef _HEADER_DIFF_ALGS
#define _HEADER_DIFF_ALGS 1
#include "cfile.h"
#include "dcbuffer.h"


/*struct cbuffer {
	unsigned char *buff_start;
	unsigned char *buff_end;
	unsigned char *head;
	unsigned char *pos;
	unsigned char *tail;
};

signed int initCBuff(struct cbuffer *cbuff, unsigned long buff_size);
void freeCBuff(struct cbuffer *cbuff);
void incrCBuff(struct cbuffer *cbuff);
void decrCBuff(struct cbuffer *cbuff);
void pushCBuff(struct cbuffer *cbuff, unsigned char *c, unsigned int len);
void match(struct cbuffer *cbuff, struct cfile *src, struct cfile *ver, 
	unsigned long *backwards_offset, unsigned long *len);
unsigned int matchBack(struct cbuffer *cbuff, struct cfile *src,
	struct cfile *ver);
unsigned int matchForw(struct cbuffer *cbuff, struct cfile *src,
	struct cfile *ver);
*/
/*char *OneHalfPassCorrecting(unsigned int encoding_type,
    unsigned int offset_type,
    unsigned char *ref, unsigned long ref_len,
    unsigned char *ver, unsigned long ver_len, unsigned int seed_len, 
    //int out_fh struct cfile *out_fh);*/
 
signed int OneHalfPassCorrecting(unsigned int encoding_type,
    unsigned int offset_type, struct cfile *ref_cfh, 
    struct cfile *ver_cfh, struct cfile *out_cfh,
    unsigned int seed_len);


#endif
