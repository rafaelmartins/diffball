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
#ifndef _HEADER_HASH
#define _HEADER_HASH 1
#include "cfile.h"
#include "adler32.h"
#include "defs.h"

#define DEFAULT_SEED_LEN 	(16)
#define DEFAULT_MAX_HASH_COUNT	(64000000/sizeof(unsigned long))
#define RH_MOD_HASH	(0x1)
#define RH_SORT_HASH	(0x2)

typedef struct {
    unsigned long chksum;
    off_u64       offset;
} chksum_ent;

typedef struct {
    unsigned int seed_len;
    unsigned long hr_size;
    unsigned char type;
    union {
	unsigned long *mod;
        chksum_ent *chk;
    } hash;
    unsigned int  sample_rate;
    cfile *ref_cfh;
    unsigned long inserts;
    unsigned long duplicates;
#ifdef DEBUG_HASH
    unsigned long bad_duplicates;
#endif
} RefHash;

inline unsigned long hash_it(RefHash *rhash, ADLER32_SEED_CTX *ads);
inline unsigned long get_offset(RefHash *rhash, unsigned long index);

signed int init_RefHash(RefHash *rhash, cfile *ref_cfh, 
	unsigned int seed_len, unsigned int sample_rate, 
	unsigned long hr_size, unsigned int hash_type);
signed int free_RefHash(RefHash *rhash);	
void print_RefHash_stats(RefHash *rhash);
#endif

