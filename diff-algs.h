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
#ifndef _HEADER_DIFF_ALGS
#define _HEADER_DIFF_ALGS 1

#define DEFAULT_SEED_LEN 	(16)
#define COMPUTE_SAMPLE_RATE(hs, x)		\
   ((x) > (hs) ? MAX(1,((x)/(hs))-.5) : 1)
#define MULTIPASS_GAP_KLUDGE    (1.25)

#include "cfile.h"
#include "dcbuffer.h"
#include "hash.h"

void print_RefHash_stats(RefHash *rhash);
signed int OneHalfPassCorrecting(CommandBuffer *buffer, RefHash *rhash, unsigned char src_id, 
	cfile *ver_cfh, unsigned char ver_id);
signed int MultiPassAlg(CommandBuffer *buffer, cfile *ref_cfh, unsigned char ref_id, 
    cfile *ver_cfh, unsigned char ver_id, 
    unsigned long max_hash_size);
#endif

