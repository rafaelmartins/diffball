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
typedef struct _RefHash {
	unsigned int seed_len;
	unsigned long hr_size;
	unsigned long *hash;
	unsigned int  sample_rate;
	cfile *ref_cfh;
	unsigned long inserts;
	unsigned long duplicates;
} RefHash;

signed int init_RefHash(RefHash *rhash, cfile *ref_cfh, 
	unsigned int seed_len, unsigned int sample_rate, 
	unsigned long hr_size);
	
signed int OneHalfPassCorrecting(CommandBuffer *buffer, 
	RefHash *rhash, cfile *ver_cfh);

#endif

