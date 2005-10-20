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
#include <stdlib.h>
#include <diffball/defs.h>
#include <errno.h>
#include <string.h>
#include <diffball/adler32.h>
#include <diffball/primes.h>
#include <diffball/defs.h>
#include <diffball/hash.h>

int 
cmp_chksum_ent(const void *ce1, const void *ce2)
{
	chksum_ent *c1 = (chksum_ent *)ce1;
	chksum_ent *c2 = (chksum_ent *)ce2;
	return (c1->chksum==c2->chksum ? 0 : 
		c1->chksum < c2->chksum ? -1 : 1);
}


inline signed int
RHash_cleanse(RefHash *rh)
{
	if(rh->cleanse_hash)
		return rh->cleanse_hash(rh);
	return 0;
}

inline signed int
RHash_sort(RefHash *rh)
{
	if(rh->sort_hash)
		return rh->sort_hash(rh);
	return 0;
}

/* ripped straight out of K&R C manual.  great book btw. */
signed long
RH_bucket_find_chksum(unsigned short chksum, unsigned short array[], 
	unsigned short count)
{
	signed long low,high,mid;
	assert(count);
	low = 0;
	high = count - 1;
	while(low <= high) {
		mid = (low+high) /2;
		if(chksum < array[mid])
			high = mid -1;
		else if (chksum > array[mid]) 
			low = mid + 1;
		else
			return mid;
	 }
	 return -1;
}

off_u64
rh_mod_lookup(RefHash *rhash, ADLER32_SEED_CTX *ads)
{
	return ((unsigned long *)rhash->hash)[get_checksum(ads) % rhash->hr_size];
}

off_u64
base_rh_mod_lookup(RefHash *rhash, ADLER32_SEED_CTX *ads)
{
   unsigned long checksum, index;
   checksum = get_checksum(ads);
   index = checksum % rhash->hr_size;
   return ((chksum_ent *)rhash->hash)[index].chksum == checksum ? ((chksum_ent *)rhash->hash)[index].offset : 0;
}

off_u64
base_rh_sort_lookup(RefHash *rhash, ADLER32_SEED_CTX *ads)
{
	chksum_ent ent, *p;
	ent.chksum = get_checksum(ads);
	p = (chksum_ent*)bsearch(&ent, rhash->hash, rhash->hr_size, sizeof(chksum_ent), cmp_chksum_ent);
	if(p == NULL)
		return 0;
	return p->offset;
}

off_u64
base_rh_bucket_lookup(RefHash *rhash, ADLER32_SEED_CTX *ads) {
	bucket *hash = (bucket*)rhash->hash;
	unsigned long index, chksum, pos;
	chksum = get_checksum(ads);
	index = chksum & 0xffff;
	if(hash->depth[index]==0) {
		return 0;
	}
	chksum = ((chksum >> 16) & 0xffff);
	pos = RH_bucket_find_chksum(chksum, hash->chksum[index], hash->depth[index]);
	if(pos >= 0)
		return hash->offset[index][pos];
	return 0;
}


signed int 
free_RefHash(RefHash *rhash)
{
	v2printf("free_RefHash\n");
	if(rhash->free_hash)
		rhash->free_hash(rhash);
	else if(rhash->hash)
		free(rhash->hash);
	rhash->hash = NULL;
	rhash->ref_cfh = NULL;
	rhash->free_hash = NULL;
	rhash->hash_insert = NULL;
	rhash->seed_len = rhash->hr_size = rhash->sample_rate = rhash->inserts = rhash->type = rhash->flags = rhash->duplicates = 0;
	return 0;
}

void
rh_bucket_free(RefHash *rhash)
{
	unsigned long x;
	bucket *hash = (bucket *)rhash->hash;
	for(x=0; x < rhash->hr_size; x++) {
		if(hash->chksum[x]!= NULL) {
			free(hash->chksum[x]);
			free(hash->offset[x]);
		} else {
			assert(hash->depth[x]==0);
		}
	}
	free(hash->chksum);
	free(hash->offset);
	free(hash->depth);
	free(hash);
}



void
common_init_RefHash(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned int type, 
hash_insert_func hif, free_hash_func fhf, hash_lookup_offset_func hlof)
{
	rhash->flags = 0;
	rhash->type = 0;
	rhash->seed_len = seed_len;
	assert(seed_len > 0);
	rhash->sample_rate = sample_rate;
	rhash->ref_cfh = ref_cfh;
	rhash->inserts = rhash->duplicates = 0;
	rhash->hash = NULL;
	rhash->type = type;
	rhash->hr_size = 0;
	rhash->hash_insert = hif;
	rhash->insert_match = NULL;
	rhash->free_hash   = fhf;
	rhash->sort_hash   = NULL;
	rhash->lookup_offset = hlof;
	rhash->cleanse_hash = NULL;
}


signed int
rh_mod_hash_init(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned long hr_size)
{
	unsigned long int x;
	unsigned long *hash;
	common_init_RefHash(rhash, ref_cfh, seed_len, sample_rate, RH_MOD_HASH, rh_mod_hash_insert, NULL, rh_mod_lookup);
	if((rhash->hr_size = get_nearest_prime(hr_size)) == 0)
		return MEM_ERROR;

	if((hash=(unsigned long*)malloc(sizeof(unsigned long) * (rhash->hr_size)))==NULL) {
			return MEM_ERROR;
	}

	// init the bugger==0
	for(x=0; x < rhash->hr_size; x++) {
		hash[x] = 0;
	}
	rhash->hash = (void *)hash;
	rhash->flags |= RH_SORTED;
	return 0;
}


signed int
base_rh_rmod_hash_init(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned long hr_size, unsigned int type)
{
	chksum_ent *hash;
	unsigned long x;
	common_init_RefHash(rhash, ref_cfh, seed_len, sample_rate, type, base_rh_mod_hash_insert, NULL, base_rh_mod_lookup);
	if((rhash->hr_size = get_nearest_prime(hr_size)) == 0)
		return MEM_ERROR;

	if((hash=(chksum_ent *)malloc(sizeof(chksum_ent) * (rhash->hr_size)))==NULL) {
		return MEM_ERROR;
	}

	for(x = 0; x < rhash->hr_size; x++)
		hash[x].chksum = hash[x].offset = 0;
	rhash->flags |= RH_SORTED;

	if(type == RH_RMOD_HASH) {
		rhash->flags |= RH_IS_REVLOOKUP;
		rhash->insert_match = rh_rmod_insert_match;
	}
	rhash->hash = (void*)hash;
	rhash->type = type;
	return 0;
}

signed int
base_rh_sort_hash_init(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned long hr_size, unsigned int type)
{
	common_init_RefHash(rhash, ref_cfh, seed_len, sample_rate, type, base_rh_sort_hash_insert, NULL, base_rh_sort_lookup);
	rhash->hr_size = hr_size;
	if((rhash->hash = (void *)malloc(sizeof(chksum_ent) * rhash->hr_size))==NULL) {
		return MEM_ERROR;
	}
	rhash->sort_hash = base_rh_sort_hash;
	rhash->cleanse_hash = base_rh_sort_hash;
	if(type == RH_RSORT_HASH) {
		rhash->flags |= RH_IS_REVLOOKUP;
		rhash->insert_match = rh_rsort_insert_match;
	}
	rhash->flags |= RH_SORTED;
	return 0;
}

signed int
base_rh_bucket_hash_init(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, unsigned int sample_rate, unsigned long hr_size, unsigned int type)
{
	bucket *rh;
	unsigned long x;
	common_init_RefHash(rhash, ref_cfh, seed_len, sample_rate, type, base_rh_bucket_hash_insert, rh_bucket_free, base_rh_bucket_lookup);
	rhash->hr_size = 0x10000;
	rh = (bucket*)malloc(sizeof(bucket));
	if(rh == NULL)
		return MEM_ERROR;
	rh->max_depth = 255;

	if((rh->depth = (unsigned char *)malloc(rhash->hr_size)) == NULL) {
		free(rh);
		return MEM_ERROR;
	} else if((rh->chksum = (unsigned short **)malloc(rhash->hr_size * sizeof(unsigned short *)))==NULL) {
		free(rh->depth); free(rh);
		return MEM_ERROR;
	} else if((rh->offset = (off_u64 **)malloc(rhash->hr_size * sizeof(off_u64 *)))==NULL) {
		free(rh->chksum); free(rh->depth); free(rh);
		return MEM_ERROR;
	}
	for(x=0; x < rhash->hr_size; x++) {
		rh->offset[x] = NULL;
		rh->chksum[x] = NULL;
		rh->depth[x] = 0;
	}
	rhash->hash = (void *)rh;
	if(type & RH_RBUCKET_HASH) {
		rhash->cleanse_hash = rh_rbucket_cleanse;
		rhash->flags |= RH_IS_REVLOOKUP;
		rhash->insert_match = rh_rbucket_insert_match;
	}
	return 0;
}

signed int
RH_bucket_resize(bucket *hash, unsigned short index, unsigned short size)
{
	assert(
		hash->depth[index]==0		|| 
		hash->depth[index]==4		|| 
		hash->depth[index]==8		|| 
		hash->depth[index]==16		||
		hash->depth[index]==32		||
		hash->depth[index]==64		||
		hash->depth[index]==128);
	if(hash->depth[index]==0) {
		if((hash->chksum[index] = (unsigned short *)malloc(size * sizeof(unsigned short)))==NULL)
			return MEM_ERROR;
		if((hash->offset[index] = (off_u64 *)malloc(size * sizeof(off_u64)))==NULL)
			return MEM_ERROR;
	}
	if((hash->chksum[index] = (unsigned short *)realloc(hash->chksum[index], size * sizeof(unsigned short))) == NULL)
		return MEM_ERROR;
	else if((hash->offset[index] = (off_u64 *)realloc(hash->offset[index], size * sizeof(off_u64)))==NULL)
		return MEM_ERROR;
	return 0;
}

signed int
rh_mod_hash_insert(RefHash *rhash, ADLER32_SEED_CTX *ads, off_u64 offset)
{
	unsigned long *hash = (unsigned long*) rhash->hash;
	unsigned long index =0;
	index = get_checksum(ads) % rhash->hr_size;
	if(! hash[index]) {
		hash[index] = offset;
		return SUCCESSFULL_HASH_INSERT;
	}
	return FAILED_HASH_INSERT;
}

signed int
base_rh_mod_hash_insert(RefHash *rhash, ADLER32_SEED_CTX *ads, off_u64 offset)
{
	chksum_ent *hash = (chksum_ent *)rhash->hash;
	unsigned long chksum = get_checksum(ads);
	unsigned long index = chksum % rhash->hr_size;
	if(! hash[index].chksum) {
		hash[index].chksum = chksum;
		/* cmod == complete hash, not reverse lookups.  so the offset gets recorded */
		if(rhash->type & RH_CMOD_HASH)
			hash[index].offset = offset;
		return SUCCESSFULL_HASH_INSERT;
	}
	return FAILED_HASH_INSERT;
}


signed int
base_rh_bucket_hash_insert(RefHash *rhash, ADLER32_SEED_CTX *ads, off_u64 offset)
{
	unsigned long chksum, index;
	signed   int low, high, mid;
	bucket *hash;
	hash = (bucket *)rhash->hash;
	chksum = get_checksum(ads);
	index = chksum & 0xffff;
	chksum = ((chksum >>16) & 0xffff);
	if(! hash->depth[index]) {
		if(RH_bucket_resize(hash, index, RH_BUCKET_MIN_ALLOC)) {
			return MEM_ERROR;
		}
		hash->chksum[index][0] = chksum;
		if(rhash->type & (RH_BUCKET_HASH)) {
			hash->offset[index][0] = offset;
		} else {
			hash->offset[index][0] = 0;
		}
		hash->depth[index]++;
		return SUCCESSFULL_HASH_INSERT;
	} else if(hash->depth[index] < hash->max_depth) {
		low = 0;
		high = hash->depth[index] - 1;
		while(low < high) {
			mid = (low + high) /2;
			if(chksum < hash->chksum[index][mid])
				high = mid -1;
			else if (chksum > hash->chksum[index][mid]) 
				low = mid + 1;
			else {
				low = mid;
				break;
			}
		}
		if(hash->chksum[index][low] != chksum) {
			/* expand bucket if needed */

#define NEED_RESIZE(x)														\
	((x)==128 || (x)==64 || (x)==32 || (x)==16 || (x)==8 || (x)==4)

			if(NEED_RESIZE(hash->depth[index])) {
				if (RH_bucket_resize(hash, index, MIN(hash->max_depth, (hash->depth[index] << 1)))) {
					return MEM_ERROR;
				}
			}
			if(hash->chksum[index][low] < chksum) {
				/* shift low 1 element to the right */
				memmove(hash->chksum[index] + low + 1, hash->chksum[index] + low, (hash->depth[index] - low) * sizeof(unsigned short));
				hash->chksum[index][low] = chksum;
				if(rhash->type & RH_BUCKET_HASH) {
					memmove(hash->offset[index] + low + 1, hash->offset[index] + low , (hash->depth[index] - low) * sizeof(off_u64));
					hash->offset[index][low] = offset;
				} else {
					hash->offset[index][hash->depth[index]] = 0;
				}
			 } else if(low == hash->depth[index] -1) {
				hash->chksum[index][hash->depth[index]] = chksum;
				if(rhash->type & RH_BUCKET_HASH) {
					hash->offset[index][hash->depth[index]] = offset;
				} else {
					hash->offset[index][hash->depth[index]] = 0;
				}
			} else {
				memmove(hash->chksum[index] + low + 2, hash->chksum[index] + low +1 , (hash->depth[index] - low - 1) * sizeof(unsigned short));
				hash->chksum[index][low] = chksum;
				if(rhash->type & RH_BUCKET_HASH) {
					memmove(hash->offset[index] + low + 2, hash->offset[index] + low +1 , (hash->depth[index] - low - 1) * sizeof(off_u64));
					/* this ought to be low + 1 */
					hash->offset[index][low + 1] = offset;
				} else {
					hash->offset[index][hash->depth[index]] = 0;
				}
			}
			hash->depth[index]++;
			if(rhash->inserts + 1 == (rhash->hr_size * hash->max_depth))
				return SUCCESSFULL_HASH_INSERT_NOW_IS_FULL;
			return SUCCESSFULL_HASH_INSERT;
		}
	}
	return FAILED_HASH_INSERT;
}

signed int
base_rh_sort_hash_insert(RefHash *rhash, ADLER32_SEED_CTX *ads, off_u64 offset)
{
	chksum_ent *hash = (chksum_ent *)rhash->hash;
	if(rhash->hr_size == rhash->inserts) {
		v1printf("resizing from %lu to %lu\n", rhash->hr_size, rhash->hr_size + 1000);
		if((hash = (chksum_ent *)realloc(rhash->hash, (rhash->hr_size + 1000) * sizeof(chksum_ent)))==NULL){
			return MEM_ERROR;
		}
		rhash->hash = (void *)hash;
		rhash->hr_size +=1000;
	}
	hash[rhash->inserts].chksum = get_checksum(ads);
	hash[rhash->inserts].offset = (rhash->type == RH_SORT_HASH ? offset : 0);
	return SUCCESSFULL_HASH_INSERT;
}

signed int
RHash_insert_block(RefHash *rhash, cfile *ref_cfh, off_u64 ref_start, off_u64 ref_end)
{
	return internal_loop_block(rhash, ref_cfh, ref_start, ref_end, rhash->hash_insert);
}

signed int
RHash_find_matches(RefHash *rhash, cfile *ref_cfh, off_u64 ref_start, off_u64 ref_end)
{
	if(rhash->flags & ~RH_IS_REVLOOKUP)
		return 0;
	if(rhash->sort_hash) {
		signed int x = rhash->sort_hash(rhash);
		if(x)
			return x;
	}
	return internal_loop_block(rhash, ref_cfh, ref_start, ref_end, rhash->insert_match);
}

signed int
internal_loop_block(RefHash *rhash, cfile *ref_cfh, off_u64 ref_start, off_u64 ref_end, hash_insert_func hif)
{
	ADLER32_SEED_CTX ads;
	unsigned long index, skip=0;
	unsigned long len;
	signed int		 result;
	cfile_window *cfw;
	init_adler32_seed(&ads, rhash->seed_len, 1);
	cseek(ref_cfh, ref_start, CSEEK_FSTART);
	cfw = expose_page(ref_cfh);
	if(cfw == NULL)
		return IO_ERROR;
	if(cfw->end==0) {
		return 0;
	}

	if(cfw->pos + rhash->seed_len < cfw->end) {
		update_adler32_seed(&ads, cfw->buff + cfw->pos, rhash->seed_len);
		cfw->pos += rhash->seed_len;
	} else {
		len = rhash->seed_len;
		while(len) {
			skip = MIN(cfw->end - cfw->pos, len);
			update_adler32_seed(&ads, cfw->buff + cfw->pos, skip);
			len -= skip;
			if(len)
				cfw = next_page(ref_cfh);
			else
				cfw->pos += skip;
			if(cfw == NULL || cfw->end==0) {
				return EOF_ERROR;
			}
		}
	}

	while(cfw->offset + cfw->pos <= ref_end) {
		if(cfw->pos > cfw->end) {
			cfw = next_page(ref_cfh);
			if(cfw == NULL || cfw->end==0) {
				return MEM_ERROR;
			}
		}
		skip=0;
		len=1;
		result = hif(rhash, &ads, cfw->offset + cfw->pos - rhash->seed_len);
		if(result < 0) {
			return result;
		} else if (result == SUCCESSFULL_HASH_INSERT) {
			rhash->inserts++;
			if(rhash->sample_rate <= 1) {
					len = 1;
			} else if(rhash->sample_rate > rhash->seed_len) {
					len = rhash->seed_len;
				skip = rhash->sample_rate - rhash->seed_len;
			} else if (rhash->sample_rate > 1){
				len = rhash->sample_rate;
			}
		} else if (result == FAILED_HASH_INSERT) {
			rhash->duplicates++;
		} else if(result == SUCCESSFULL_HASH_INSERT_NOW_IS_FULL) {
			rhash->inserts++;
			free_adler32_seed(&ads);
			return 0;
		}
		if(cfw->pos + cfw->offset + skip + len > ref_end) {
			break;
		}

		/* position ourself */
		while(cfw->pos + skip >= cfw->end) {
			skip -= (cfw->end - cfw->pos);
			cfw = next_page(ref_cfh);
			if(cfw == NULL) {
				free_adler32_seed(&ads);
				return IO_ERROR;
			}
		}
		cfw->pos += skip;

		// loop till we've updated the chksum.
		while(len) {
			index = MIN(cfw->end - cfw->pos, len);
			update_adler32_seed(&ads, cfw->buff + cfw->pos, index);
			cfw->pos += index;
			len -= index;

			// get next page if we still need more
			if(len) {
				cfw = next_page(ref_cfh);
				if(cfw==NULL) {
					free_adler32_seed(&ads);
					return IO_ERROR;
				}
			}
		}
	}
	free_adler32_seed(&ads);
	return 0;
}

signed int
rh_rsort_insert_match(RefHash *rhash, ADLER32_SEED_CTX *ads, off_u64 offset)
{
	chksum_ent m_ent, *match;
	m_ent.chksum = get_checksum(ads) % rhash->hr_size;
	match = bsearch(&m_ent, rhash->hash, rhash->hr_size, sizeof(chksum_ent), cmp_chksum_ent);
	if(match != NULL && match->offset == 0) {
		match->offset = offset;
		return SUCCESSFULL_HASH_INSERT;
	}
	return FAILED_HASH_INSERT;
}	

signed int
rh_rmod_insert_match(RefHash *rhash, ADLER32_SEED_CTX *ads, off_u64 offset)
{
	unsigned long index, chksum;
	chksum = get_checksum(ads);
	index = chksum % rhash->hr_size;
	if( ((chksum_ent *)rhash->hash)[index].chksum == chksum) {
		((chksum_ent *)rhash->hash)[index].offset = offset;
		return SUCCESSFULL_HASH_INSERT;
	}
	return FAILED_HASH_INSERT;
}

signed int
rh_rbucket_insert_match(RefHash *rhash, ADLER32_SEED_CTX *ads, off_u64 offset)
{
	bucket *hash = (bucket *)rhash->hash;
	unsigned long index, chksum;
	signed long pos;
	chksum = get_checksum(ads);
	index = (chksum & 0xffff);
	chksum = ((chksum >> 16) & 0xffff);
	if(hash->depth[index]) {
		pos = RH_bucket_find_chksum(chksum, hash->chksum[index], hash->depth[index]);
		if(pos >= 0 && hash->offset[index][pos]==0) {
			hash->offset[index][pos] = offset;
			return SUCCESSFULL_HASH_INSERT;
		}
	}
	return FAILED_HASH_INSERT;
}

signed int
base_rh_sort_hash(RefHash *rhash)
{
	unsigned long old_chksum, x=0, hash_offset=0;
	chksum_ent *hash = (chksum_ent *)rhash->hash;
	assert(rhash->inserts);
	v1printf("inserts=%lu, hr_size=%lu\n", rhash->inserts, rhash->hr_size);
	qsort(hash, rhash->inserts, sizeof(chksum_ent), cmp_chksum_ent);
	old_chksum = hash[0].chksum;
	rhash->duplicates=0;
	for(x=1; x < rhash->inserts; x++) {
		if(hash[x].chksum==old_chksum) {
			rhash->duplicates++;
		} else {
			old_chksum = hash[x].chksum;
			if(hash_offset) {
				hash[x - rhash->duplicates].chksum = old_chksum;
				hash[x - rhash->duplicates].offset = hash[x].offset;
			}
		}
	}
	rhash->inserts -= rhash->duplicates;
	if((rhash->hash = (void *)realloc(rhash->hash, rhash->inserts * sizeof(chksum_ent)))==NULL) {
		return MEM_ERROR;
	}
	rhash->hr_size = rhash->inserts;
	v1printf("hash is %lu bytes\n", rhash->hr_size * sizeof(chksum_ent));
	rhash->flags |= RH_SORTED;
	return 0;
}

signed int
base_rh_sort_cleanse(RefHash *rhash)
{
	unsigned long x=0, hash_offset=0;
	chksum_ent *hash = (chksum_ent *)rhash->hash;
	assert(rhash->inserts);
	assert(rhash->flags & RH_SORTED);
	v1printf("inserts=%lu, hr_size=%lu\n", rhash->inserts, rhash->hr_size);
	rhash->duplicates=0;
	for(x=1; x < rhash->inserts; x++) {
		if(hash[x].offset==0) {
			rhash->duplicates++;
		} else {
			if(hash_offset) {
				hash[x - rhash->duplicates].chksum = hash[x].chksum;
				hash[x - rhash->duplicates].offset = hash[x].offset;
			}
		}
	}
	rhash->inserts -= rhash->duplicates;
	rhash->hr_size = rhash->inserts;
	return 0;
}

signed int
rh_rbucket_cleanse(RefHash *rhash)
{
	bucket *hash = (bucket*)rhash->hash;
	unsigned long x=0, y=0, shift=0;
	rhash->inserts = 0;
	for(x=0; x < rhash->hr_size; x++) {
		if(hash->depth[x] > 0) {
			shift=0;
			for(y=0; y < hash->depth[x]; y++) {
				if(hash->offset[x][y]==0) {
					shift++;
				} else if(shift) {
					hash->offset[x][y - shift] = hash->offset[x][y];
					hash->chksum[x][y - shift] = hash->chksum[x][y];
				}
			}
			hash->depth[x] -= shift;
			if(hash->depth[x]==0) {
				free(hash->chksum[x]);
				free(hash->offset[x]);
				hash->chksum[x] = NULL;
				hash->offset[x] = NULL;
				continue;
			}
			rhash->inserts += hash->depth[x];
			if((hash->chksum[x] = (unsigned short *) realloc(hash->chksum[x], sizeof(unsigned short) * hash->depth[x])) == NULL || 
				(hash->offset[x] = (off_u64 *) realloc(hash->offset[x], sizeof(off_u64) * hash->depth[x])) == NULL) {
				return MEM_ERROR;
			}
		}
	}
	rhash->flags |= RH_FINALIZED;
	return 0;
}

void
print_RefHash_stats(RefHash *rhash) {
	v1printf("hash stats: inserts(%lu), duplicates(%lu), hash size(%lu)\n",
		rhash->inserts, rhash->duplicates, rhash->hr_size);
	v1printf("hash stats: load factor(%f%%)\n", 
		((float)rhash->inserts/rhash->hr_size* 100));
	v1printf("hash stats: duplicate rate(%f%%)\n", 
		((float)rhash->duplicates/(rhash->inserts + rhash->duplicates) * 100));
#ifdef DEBUG_HASH
	v1printf("hash stats: bad duplicates(%f%%)\n",((float)
		rhash->bad_duplicates/rhash->duplicates * 100));
	v1printf("hash stats: good duplicates(%f%%)\n", 100.0 - ((float)
		rhash->bad_duplicates/rhash->duplicates * 100));
#endif
/*	if(rhash->type & RH_RSORT_HASH) {
		for(x=0; x < rhash->hr_size; x++) {
			if(rhash->hash.chk[x].offset) {
				matched++;
			}
		}
		v1printf("hash stats: matched entries(%lu), percentage(%f%%)\n", 
			matched, ((float)matched/rhash->inserts)*100);
	}
*/
	v1printf("hash stats: seed_len(%u), sample_rate(%u)\n", rhash->seed_len,
		rhash->sample_rate);
}
