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
#ifndef _HEADER_HASH
#define _HEADER_HASH 1
#include <cfile.h>
#include <diffball/adler32.h>
#include <diffball/defs.h>

#define DEFAULT_SEED_LEN		 (16)
#define DEFAULT_MAX_HASH_COUNT		(48000000/sizeof(unsigned long))

#define RH_MOD_HASH				(0x1)
#define RH_RMOD_HASH				(0x2)
#define RH_CMOD_HASH				(0x4)
#define RH_SORT_HASH				(0x8)
#define RH_RSORT_HASH				(0x10)
#define RH_BUCKET_HASH				(0x20)
#define RH_RBUCKET_HASH				(0x40)

#define RH_BUCKET_MIN_ALLOC (4)

#define SUCCESSFULL_HASH_INSERT_NOW_IS_FULL		2
#define SUCCESSFULL_HASH_INSERT						1
#define FAILED_HASH_INSERT						0

#define RH_IS_RLOOKUP_HASH(rh)												 \
	 ((rh)->type & (RH_RBUCKET_HASH | RH_RSORT_HASH | RH_RMOD_HASH))

#define RH_FINALIZED		(0x1)
#define RH_SORTED		(0x2)
#define RH_IS_REVLOOKUP		(0x4)

typedef struct {
	unsigned long		chksum;
	off_u64				offset;
} chksum_ent;

typedef struct ll_chksum_ent ll_chksum_ent;
struct ll_chksum_ent {
	chksum_ent ent;
	ll_chksum_ent *next;
};

typedef struct {
	unsigned char		*depth;
	unsigned short		**chksum;
	off_u64				**offset;
	unsigned short		max_depth;
} bucket;

typedef struct _RefHash *RefHash_ptr;

typedef signed int (*hash_insert_func)(RefHash_ptr, ADLER32_SEED_CTX *, off_u64);
typedef void (*free_hash_func)(RefHash_ptr);
typedef signed int (*cleanse_hash_func)(RefHash_ptr);
typedef cleanse_hash_func sort_hash_func;
typedef void (*reverse_lookups_hash_func)(RefHash_ptr, cfile *);
typedef off_u64 (*hash_lookup_offset_func)(RefHash_ptr, ADLER32_SEED_CTX *);

typedef struct _RefHash {
	unsigned int		 seed_len;
	unsigned long		 hr_size;
	unsigned char		 type;
	unsigned char		 flags;
	hash_insert_func		 hash_insert;
	hash_insert_func		insert_match;
	free_hash_func		free_hash;
	sort_hash_func		sort_hash;
	cleanse_hash_func		cleanse_hash;
	hash_lookup_offset_func		lookup_offset;
	void *				hash;
	unsigned int  sample_rate;
	cfile *ref_cfh;
	unsigned long inserts;
	unsigned long duplicates;
} RefHash;


#define FIND_NEAREST_PRIME_HR(hr_size)		 \
PRIME_CTX pctx;								\
init_primes(pctx);						\




signed int 
init_RefHash(RefHash *rhash, cfile *ref_cfh, 
		unsigned int seed_len, unsigned int sample_rate, 
		unsigned long hr_size, unsigned int hash_type);

signed int 
RHash_insert_block(RefHash *rhash, cfile *ref_cfh, off_u64 ref_start, off_u64 ref_end);
signed int 
internal_loop_block(RefHash *rhash, cfile *ref_cfh, off_u64 ref_start, off_u64 ref_end, hash_insert_func);

signed int
RHash_find_matches(RefHash *rhash, cfile *ref_cfh, off_u64 ref_start, off_u64 ref_end);

inline signed int RHash_sort(RefHash *rhash);
inline signed int RHash_cleanse(RefHash *rhash);
signed int free_RefHash(RefHash *rhash);		
void print_RefHash_stats(RefHash *rhash);

signed int
RH_bucket_resize(bucket *hash, unsigned short index, unsigned short size);


//hash type initializations.
void 
common_init_RefHash(RefHash *, cfile *, unsigned int, unsigned int, unsigned int, hash_insert_func hif, free_hash_func fhf,
	hash_lookup_offset_func hlof);

signed int
rh_mod_hash_init(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned long hr_size);

signed int
base_rh_rmod_hash_init(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned long hr_size, 
	unsigned int type);

#define rh_rmod_hash_init(rh,rc,sl,sr,hr)		\
	base_rh_rmod_hash_init((rh),(rc),(sl),(sr),(hr), RH_RMOD_HASH)

#define rh_cmod_hash_init(rh,rc,sl,sr,hr)		\
	base_rh_rmod_hash_init((rh),(rc),(sl),(sr),(hr), RH_CMOD_HASH)

signed int
base_rh_sort_hash_init(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned long hr_size, 
	unsigned int type);

signed int
base_rh_bucket_hash_init(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned long hr_size, 
    unsigned int type);

#define rh_sort_hash_init(rh,rc,sl,sr,hr)		\
	base_rh_sort_hash_init((rh),(rc),(sl),(sr),(hr), RH_SORT_HASH)

#define rh_rsort_hash_init(rh,rc,sl,sr,hr)		\
	base_rh_sort_hash_init((rh),(rc),(sl),(sr),(hr), RH_RSORT_HASH)

signed int
base_bucket_hash_init(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned long hr_size, 
	unsigned int type);

#define rh_bucket_hash_init(rh,rc,sl,sr,hr)		\
	base_rh_bucket_hash_init((rh),(rc),(sl),(sr),(hr), RH_BUCKET_HASH)

#define rh_rbucket_hash_init(rh,rc,sl,sr,hr)		\
	base_rh_bucket_hash_init((rh),(rc),(sl),(sr),(hr), RH_RBUCKET_HASH)

signed int
base_rh_sort_hash(RefHash *rhash);
signed int
rh_rbucket_cleanse(RefHash *rhash);

signed int
rh_mod_hash_insert(RefHash *, ADLER32_SEED_CTX *, off_u64);

signed int
base_rh_mod_hash_insert(RefHash *, ADLER32_SEED_CTX *, off_u64);

signed int
base_rh_sort_hash_insert(RefHash *, ADLER32_SEED_CTX *, off_u64);

signed int
rh_rmod_insert_match(RefHash *, ADLER32_SEED_CTX *, off_u64);

signed int
rh_rsort_insert_match(RefHash *, ADLER32_SEED_CTX *, off_u64);

signed int
rh_rbucket_insert_match(RefHash *, ADLER32_SEED_CTX *, off_u64);

signed int
base_rh_bucket_hash_insert(RefHash *, ADLER32_SEED_CTX *, off_u64);

#define lookup_offset(rh, ads)		(rh)->lookup_offset((rh),(ads))

off_u64
rh_mod_lookup(RefHash *, ADLER32_SEED_CTX *);

off_u64
base_rh_mod_lookup(RefHash *, ADLER32_SEED_CTX *);

off_u64
base_rh_sort_lookup(RefHash *, ADLER32_SEED_CTX *);

off_u64
base_rh_bucket_lookup(RefHash *, ADLER32_SEED_CTX *);


#endif

