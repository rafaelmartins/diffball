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
#include <errno.h>
#include <string.h>
#include "adler32.h"
#include "primes.h"
#include "defs.h"
#include "hash.h"

int 
cmp_chksum_ent(const void *ce1, const void *ce2)
{
    chksum_ent *c1 = (chksum_ent *)ce1;
    chksum_ent *c2 = (chksum_ent *)ce2;
    return (c1->chksum==c2->chksum ? 0 : 
	c1->chksum < c2->chksum ? -1 : 1);
}

/* ripped straight out of K&R C manual.  great book btw. */
signed long
RH_bucket_find_chksum(unsigned short chksum, unsigned short array[], 
    unsigned short count)
{
    assert(rhash->type & (RH_BUCKET_HASH | RH_RBUCKET_HASH));
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

inline unsigned long
lookup_offset(RefHash *rhash, ADLER32_SEED_CTX *ads)
{
    chksum_ent ent;
    unsigned long chksum, index;
    signed long pos;
    void *p;
    chksum = get_checksum(ads);
    if(rhash->type & RH_MOD_HASH) {
	return (rhash->hash.mod[chksum % rhash->hr_size]);
    } else if (rhash->type & (RH_RMOD_HASH | RH_CMOD_HASH)) {
	index = chksum % rhash->hr_size;
	return (rhash->hash.chk[index].chksum == chksum ? 
	   rhash->hash.chk[index].offset : 0);
    } else if (rhash->type & (RH_SORT_HASH | RH_RSORT_HASH)) {
	ent.chksum = chksum;
	p = bsearch(&ent, rhash->hash.chk, rhash->hr_size, sizeof(chksum_ent), 
	    cmp_chksum_ent);
	if(p==NULL)
	    return 0;
	return ((chksum_ent*)p)->offset;
    } else if(rhash->type & (RH_BUCKET_HASH | RH_RBUCKET_HASH)) {
	index = chksum & 0xffff;
	if(rhash->hash.bucket.depth[index]==0) {
	    return 0;
	}
	chksum = ((chksum >> 16) & 0xffff);
	pos = RH_bucket_find_chksum(chksum, rhash->hash.bucket.chksum[index], 
	    rhash->hash.bucket.depth[index]);
	if(pos >= 0)
	    return rhash->hash.bucket.offset[index][pos];
    }	
    return 0;
}

/* this is kind of stupid I realize, but certain hashing methods return 
   different dependant on the hash state. 
   DEPRECATED also, this is disapearing shortly.  */
inline unsigned long
get_offset(RefHash *rhash, unsigned long index)
{
    chksum_ent ent;
    void *p;
    if(rhash->type & RH_MOD_HASH) {
	return (rhash->hash.mod[index]);
    } else if (rhash->type & (RH_RMOD_HASH | RH_CMOD_HASH)) {
	return (rhash->hash.chk[index].chksum ?	
	    rhash->hash.chk[index].offset : 0);
    } else if (rhash->type & (RH_SORT_HASH | RH_RSORT_HASH)) {
	ent.chksum = index;
	p = bsearch(&ent, rhash->hash.chk, rhash->hr_size, sizeof(chksum_ent), 
	    cmp_chksum_ent);
	if(p==NULL)
	    return 0;
	return ((chksum_ent*)p)->offset;
    }
    return 0;
}


/* this is on it's way out, have it gone prior to stable 0.4 */
inline unsigned long 
hash_it(RefHash *rhash, ADLER32_SEED_CTX *ads)
{
    if(rhash->type & (RH_MOD_HASH | RH_RMOD_HASH | RH_CMOD_HASH)) {
	return (get_checksum(ads) % rhash->hr_size);
//  could use just a bitmask, although doesn't perform quite as well.
//	return (get_checksum(ads) & rhash->hr_size);
    }
    return get_checksum(ads);
}

signed int 
free_RefHash(RefHash *rhash)
{
    unsigned long x;
    v2printf("free_RefHash\n");
    if((rhash->type & RH_MOD_HASH) && (rhash->hash.mod != NULL)) {
	free(rhash->hash.mod);
    } else if((rhash->type & (RH_SORT_HASH | RH_RSORT_HASH | 
	RH_RMOD_HASH | RH_CMOD_HASH)) && (rhash->hash.chk != NULL)) {
	free(rhash->hash.chk);
    } else if(rhash->type & (RH_BUCKET_HASH | RH_RBUCKET_HASH)) {
	for(x=0; x < rhash->hr_size; x++) {
	    if(rhash->hash.bucket.chksum[x]!= NULL) {
		free(rhash->hash.bucket.chksum[x]);
		free(rhash->hash.bucket.offset[x]);
	    } else {
		assert(rhash->hash.bucket.depth[x]==0);
	    }
	}
	free(rhash->hash.bucket.chksum);
	free(rhash->hash.bucket.offset);
	free(rhash->hash.bucket.depth);
	rhash->hash.bucket.chksum = NULL;
	rhash->hash.bucket.offset = NULL;
	rhash->hash.bucket.depth = NULL;
	rhash->hash.bucket.max_depth = 0;
    }
    rhash->ref_cfh = NULL;
    rhash->seed_len = rhash->hr_size = rhash->sample_rate = 
	rhash->inserts = rhash->type = rhash->flags = rhash->duplicates = 0;
    return 0;
}

signed int 
init_RefHash(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, 
    unsigned int sample_rate, unsigned long hr_size, unsigned int hash_type)
{
    off_u64 x;
    PRIME_CTX pctx;
    assert(seed_len);
    rhash->flags = 0;
    rhash->type = hash_type;
    v2printf("init_RefHash\n");
    if(rhash->type & (RH_BUCKET_HASH | RH_RBUCKET_HASH)) {
	rhash->hr_size = 0x10000;
	rhash->hash.bucket.max_depth = 255;
    } else {
	init_primes(&pctx);
	rhash->hr_size = get_nearest_prime(&pctx, hr_size);
	free_primes(&pctx);
    }
    rhash->sample_rate = sample_rate;
    rhash->ref_cfh = ref_cfh;
    rhash->seed_len = seed_len;
    rhash->inserts = rhash->duplicates = 0;
    if(rhash->type & RH_MOD_HASH) {
	if((rhash->hash.mod=(unsigned long*)malloc(sizeof(unsigned long) * 
	    (rhash->hr_size)))==NULL) {
		perror("Shite.  couldn't allocate needed memory for reference hash table.\n");
		exit(EXIT_FAILURE);
	}
	// init the bugger==0
	for(x=0; x < rhash->hr_size; x++) {
	    rhash->hash.mod[x] = 0;
	}
	rhash->flags |= RH_SORTED;
    } else if(rhash->type & (RH_RMOD_HASH | RH_CMOD_HASH)) {
	if((rhash->hash.chk=(chksum_ent*)malloc(sizeof(chksum_ent) * 
	    (rhash->hr_size)))==NULL) {
	    perror("Shite.  couldn't allocate needed memory for reference hash table.\n");
	    exit(EXIT_FAILURE);
	}
	// init the bugger==0
	for(x=0; x < rhash->hr_size; x++) {
	    rhash->hash.chk[x].chksum = rhash->hash.chk[x].offset = 0;
	}
	rhash->flags |= RH_SORTED;
    } else if (rhash->type & RH_SORT_HASH){
	if((rhash->hash.chk = (chksum_ent *)malloc(sizeof(chksum_ent) * 
	    rhash->hr_size))==NULL) {
	    perror("shite, couldn't alloc needed memory for ref hash\n");
	    exit(EXIT_FAILURE);
	}
    } else if (rhash->type & RH_RSORT_HASH) {
	if((rhash->hash.chk = (chksum_ent *)malloc(sizeof(chksum_ent) *
	    rhash->hr_size))==NULL) {
	    perror("couldn't alloc needed memory for ref hash\n");
	    exit(EXIT_FAILURE);
	}
    } else if(rhash->type & (RH_BUCKET_HASH | RH_RBUCKET_HASH)) {
	if((rhash->hash.bucket.depth = (unsigned char *)malloc(
	    rhash->hr_size)) == NULL || 
	    (rhash->hash.bucket.chksum = (unsigned short **)malloc(
	    rhash->hr_size * sizeof(unsigned short *)))==NULL ||
	    (rhash->hash.bucket.offset = (off_u64 **)malloc(
	    rhash->hr_size * sizeof(off_u64 *)))==NULL) {
	    perror("shite, couldn't alloc needed memory for ref hash\n");
	    exit(EXIT_FAILURE);
	}
	for(x=0; x < rhash->hr_size; x++) {
	    rhash->hash.bucket.offset[x] = NULL;
	    rhash->hash.bucket.chksum[x] = NULL;
	    rhash->hash.bucket.depth[x] = 0;
	}
    }
    return 0;
}

signed int
RH_bucket_resize(RefHash *rhash, unsigned short index, unsigned short size)
{
    assert(rhash->hash.bucket.depth[index]==0 || 
	rhash->hash.bucket.depth[index]==4    || 
	rhash->hash.bucket.depth[index]==8    || 
	rhash->hash.bucket.depth[index]==16   ||
	rhash->hash.bucket.depth[index]==32   ||
	rhash->hash.bucket.depth[index]==64   ||
	rhash->hash.bucket.depth[index]==128);
    if(rhash->hash.bucket.depth[index]==0) {
	return((rhash->hash.bucket.chksum[index] = (unsigned short *)malloc(
	    size * sizeof(unsigned short)))==NULL || 
	    (rhash->hash.bucket.offset[index] = (off_u64 *)malloc(size * 
	    sizeof(off_u64)))==NULL);
    }
    return((rhash->hash.bucket.chksum[index] = (unsigned short *)realloc(
	rhash->hash.bucket.chksum[index], 
	size * sizeof(unsigned short))) ==
	NULL || 
	(rhash->hash.bucket.offset[index] = (off_u64 *)realloc(
	rhash->hash.bucket.offset[index], 
	size * sizeof(off_u64)))==NULL);
}

signed int
RHash_insert_block(RefHash *rhash, cfile *ref_cfh, off_u64 ref_start, 
    off_u64 ref_end)
{
    ADLER32_SEED_CTX ads;
    unsigned long index, skip=0, chksum;
    signed long low, mid, high;
    unsigned char worth_continuing;
    cfile_window *cfw;
    unsigned int missed;

    init_adler32_seed(&ads, rhash->seed_len, 1);
    cseek(ref_cfh, ref_start, CSEEK_FSTART);
    cfw = expose_page(ref_cfh);
    if(cfw->end==0) {
	return 0;
    }

    update_adler32_seed(&ads, cfw->buff + cfw->pos, rhash->seed_len);
    missed=0;
    worth_continuing=1;
    for(cfw->pos += rhash->seed_len; cfw->offset + cfw->pos < ref_end && 
	worth_continuing; 
	cfw->pos++) {
	if(cfw->pos >= cfw->end) {
	    cfw = next_page(ref_cfh);
	    if(cfw->end==0) {
		abort();
	    }
	}

	if(rhash->type & RH_MOD_HASH) {
	    index=hash_it(rhash, &ads);
	    if(get_offset(rhash,index)==0) {
		rhash->inserts++;
		rhash->hash.mod[index] = cfw->offset + cfw->pos - 
		    rhash->seed_len;
		if(rhash->sample_rate > 1 && missed < rhash->sample_rate) {
		    skip = rhash->sample_rate - missed;
		}
		missed=0;
	    } else {
		rhash->duplicates++;
		missed++;
	    }
	} else if(rhash->type & (RH_RMOD_HASH | RH_CMOD_HASH)) {
	    chksum = get_checksum(&ads);
	    index = chksum % rhash->hr_size;
	    if(rhash->hash.chk[index].chksum==0) {
		rhash->hash.chk[index].chksum = chksum;
		rhash->inserts++;
		if(rhash->type & RH_CMOD_HASH)
		    rhash->hash.chk[index].offset = cfw->offset + cfw->pos -
			rhash->seed_len;
		if(rhash->sample_rate > 1 && missed < rhash->sample_rate) {
		    skip = rhash->sample_rate - missed;
		}
		missed=0;
	    } else {
		rhash->duplicates++;
		missed++;
	    }
	} else if(rhash->type & (RH_BUCKET_HASH | RH_RBUCKET_HASH)) {
	    chksum = get_checksum(&ads);
	    index = chksum & 0xffff;
	    chksum = ((chksum >> 16) & 0xffff);

	    if(rhash->hash.bucket.depth[index]==0) {
//		v0printf("initing bucket at index(%u)\n", index);
		if(RH_bucket_resize(rhash, index, RH_BUCKET_MIN_ALLOC)) {
		    perror("shite, malloc bucket failed.\n");
		    abort();
		}
		rhash->hash.bucket.chksum[index][0] = chksum;
		if(rhash->type & (RH_BUCKET_HASH)) {
		    rhash->hash.bucket.offset[index][0] = cfw->offset + 
			cfw->pos - rhash->seed_len;
		} else {
		    rhash->hash.bucket.offset[index][0] = 0;
		}
		rhash->hash.bucket.depth[index]++;
		rhash->inserts++;
		if(rhash->sample_rate > 1) 
		    skip = rhash->sample_rate;
	    } else if(rhash->hash.bucket.depth[index] < 
		rhash->hash.bucket.max_depth) {
		low = 0;
		high = rhash->hash.bucket.depth[index] - 1;
		while(low < high) {
		    mid = (low + high) /2;
		    if(chksum < rhash->hash.bucket.chksum[index][mid])
			high = mid -1;
		    else if (chksum > rhash->hash.bucket.chksum[index][mid]) 
			low = mid + 1;
		    else {
			low = mid;
			break;
		    }
		}

		if(rhash->hash.bucket.chksum[index][low] != chksum) {

		    /* expand bucket if needed */

#define NEED_RESIZE(x)							\
    ((x)==128 || (x)==64 || (x)==32 || (x)==16 || (x)==8 || (x)==4)

		    if(NEED_RESIZE(rhash->hash.bucket.depth[index])) {
			if (RH_bucket_resize(rhash, index, 
			    MIN(rhash->hash.bucket.max_depth, 
			    (rhash->hash.bucket.depth[index] << 1)))) {
			    perror("shite, realloc bucket failed.\n");
			    abort();
			}
		    }
		    if(rhash->hash.bucket.chksum[index][low] < chksum) {
			/* shift low 1 element to the right */
			memmove(rhash->hash.bucket.chksum[index] + low + 1, 
			    rhash->hash.bucket.chksum[index] + low, 
			    (rhash->hash.bucket.depth[index] - low) * 
			    sizeof(unsigned short));
			rhash->hash.bucket.chksum[index][low] = chksum;
			if(rhash->type & RH_BUCKET_HASH) {
			    memmove(rhash->hash.bucket.offset[index] + low + 1, 
				rhash->hash.bucket.offset[index] + low , 
				(rhash->hash.bucket.depth[index] - low) * 
				sizeof(off_u64));
			    rhash->hash.bucket.offset[index][low] = cfw->offset +
				cfw->pos - rhash->seed_len;
			} else {
			    rhash->hash.bucket.offset[index][
				rhash->hash.bucket.depth[index]] = 0;
			}
		    } else if(low == rhash->hash.bucket.depth[index] -1) {
			rhash->hash.bucket.chksum[index][
			    rhash->hash.bucket.depth[index]] = chksum;
			if(rhash->type & RH_BUCKET_HASH) {
			    rhash->hash.bucket.offset[index][
				rhash->hash.bucket.depth[index]] = cfw->offset
				+ cfw->pos - rhash->seed_len;
			} else {
			    rhash->hash.bucket.offset[index][
				rhash->hash.bucket.depth[index]] = 0;
			}
		    } else {
			memmove(rhash->hash.bucket.chksum[index] + low + 2, 
			    rhash->hash.bucket.chksum[index] + low +1 , 
			    (rhash->hash.bucket.depth[index] - low - 1) * 
			    sizeof(unsigned short));
			rhash->hash.bucket.chksum[index][low] = chksum;
			if(rhash->type & RH_BUCKET_HASH) {
			    memmove(rhash->hash.bucket.offset[index] + low + 2, 
				rhash->hash.bucket.offset[index] + low +1 , 
				(rhash->hash.bucket.depth[index] - low - 1) * 
				sizeof(off_u64));
			    /* this ought to be low + 1 */
			    rhash->hash.bucket.offset[index][low + 1] = 
				cfw->offset + cfw->pos - rhash->seed_len;
			} else {
			    rhash->hash.bucket.offset[index][
				rhash->hash.bucket.depth[index]] = 0;
			}
		    }
		    rhash->inserts++;
		    if(rhash->inserts == (rhash->hr_size * 
			rhash->hash.bucket.max_depth)) {
			worth_continuing=0;
		    }
		    rhash->hash.bucket.depth[index]++;
		    if(rhash->sample_rate > 1)
			skip = rhash->sample_rate;
		} else {
		    rhash->duplicates++;
		}
	    } else {
		rhash->duplicates++;
	    }
	} else if (rhash->type & RH_SORT_HASH) {
	    if(rhash->hr_size == rhash->inserts) {
		v1printf("resizing from %lu to %lu\n", rhash->hr_size, 
		    rhash->hr_size + 1000);
		if((rhash->hash.chk = (chksum_ent *)realloc(rhash->hash.chk, 
		    (rhash->hr_size + 1000) * sizeof(chksum_ent)))==NULL){
		    perror("crap.  realloc failed.\n");
		    exit(EXIT_FAILURE);
		}
		rhash->hr_size +=1000;
	    }
	    rhash->hash.chk[rhash->inserts].chksum = get_checksum(&ads);
	    rhash->hash.chk[rhash->inserts].offset = cfw->offset + 
		cfw->pos - rhash->seed_len;
	    rhash->inserts++;
	    if(rhash->sample_rate > 1)
		skip = rhash->sample_rate;
	} else if(rhash->type & RH_RSORT_HASH) {
	    if(rhash->hr_size == rhash->inserts) {
		v1printf("resizing from %lu to %lu\n", rhash->hr_size, 
		    rhash->hr_size + 1000);
		if((rhash->hash.chk = (chksum_ent *)realloc(rhash->hash.chk, 
		    (rhash->hr_size + 1000) * sizeof(chksum_ent)))==NULL){
		    perror("crap.  realloc failed.\n");
		    exit(EXIT_FAILURE);
		}
		rhash->hr_size +=1000;
	    }
	    rhash->hash.chk[rhash->inserts].chksum = get_checksum(&ads);
	    rhash->hash.chk[rhash->inserts].offset = 0;
	    rhash->inserts++;
	    if(rhash->sample_rate > 1)
		skip = rhash->sample_rate;
	}
	if(skip && worth_continuing) {
//	    v1printf("asked to skip %lu\n", skip);
	    if(cfw->pos + cfw->offset + skip>= ref_end) {
		cfw->pos += skip;
		continue;
	    }
	    if(skip > rhash->seed_len) {
		while(cfw->pos + skip - rhash->seed_len >= cfw->end) {
		    skip -= (cfw->end - cfw->pos);
		    cfw = next_page(ref_cfh);
		}
		cfw->pos += skip - rhash->seed_len;
		skip = rhash->seed_len;
	    }
	    // yes, I'm using index.  so what?
	    index = MIN(cfw->end - cfw->pos , skip);
	    update_adler32_seed(&ads, cfw->buff + cfw->pos, index);
	    cfw->pos += index;	    
	    skip -= index;
	    if(skip) {
		cfw = next_page(ref_cfh);
		update_adler32_seed(&ads, cfw->buff, skip);
		cfw->pos += skip;
		skip = 0;
	    }
	}// else {
	// fix this.
	    update_adler32_seed(&ads, cfw->buff + cfw->pos, 1);
	//}
    }
    free_adler32_seed(&ads);
    return 0;
}

signed int
RHash_find_matches(RefHash *rhash, cfile *ref_cfh)
{
    cfile_window *cfw;
    ADLER32_SEED_CTX ads;
    chksum_ent *me, m_ent;
    unsigned long chksum, index;
    signed long pos;
    if(!(rhash->type & (RH_RSORT_HASH | RH_RMOD_HASH | RH_RBUCKET_HASH))) {
	return 0;
    }
    if(!(RH_SORTED & rhash->flags))
	RHash_sort(rhash);
    cseek(ref_cfh, 0, CSEEK_FSTART);
    cfw = expose_page(ref_cfh);
    init_adler32_seed(&ads, rhash->seed_len, 1);
    update_adler32_seed(&ads, cfw->buff, rhash->seed_len);
    cfw->pos += rhash->seed_len;
    while(cfw->end) {
	if(cfw->end == cfw->pos) {
	    cfw = next_page(ref_cfh);
	    continue;
	}
	if(rhash->type & RH_RSORT_HASH) {
	    m_ent.chksum = hash_it(rhash, &ads);
	    me = bsearch(&m_ent, rhash->hash.chk, rhash->hr_size, 
		sizeof(chksum_ent), cmp_chksum_ent);
	    if(me!=NULL && me->offset==0) {
		me->offset = cfw->offset + cfw->pos - rhash->seed_len;
	    }
	} else if(rhash->type & RH_RMOD_HASH) {
	    chksum = get_checksum(&ads);
	    index = chksum % rhash->hr_size;
	    if(rhash->hash.chk[index].chksum == chksum) {
		rhash->hash.chk[index].offset = cfw->offset + cfw->pos - 
		    rhash->seed_len;
	    }
	} else if(rhash->type & RH_RBUCKET_HASH) {
	    chksum = get_checksum(&ads);
	    index = (chksum & 0xffff);
	    chksum = ((chksum >> 16) & 0xffff);
	    if(rhash->hash.bucket.depth[index]) {
		pos = RH_bucket_find_chksum(chksum, 
		    rhash->hash.bucket.chksum[index], 
		    rhash->hash.bucket.depth[index]);
		if(pos >= 0 && rhash->hash.bucket.offset[index][pos]==0) {
		    rhash->hash.bucket.offset[index][pos] = cfw->offset + 
			cfw->pos - rhash->seed_len;
		}
	    }
	}	    
	update_adler32_seed(&ads, cfw->buff + cfw->pos, 1);
	cfw->pos++;

    }
    return 0;
}

signed int
RHash_sort(RefHash *rhash)
{
    unsigned long old_chksum, x=0, hash_offset=0;
    assert(rhash->inserts);
    if(rhash->type & RH_SORT_HASH) {
	v1printf("inserts=%lu, hr_size=%lu\n", rhash->inserts, rhash->hr_size);
	qsort(rhash->hash.chk, rhash->inserts, sizeof(chksum_ent), 
	    cmp_chksum_ent);
	old_chksum = rhash->hash.chk[0].chksum;
	rhash->duplicates=0;
	for(x=1; x < rhash->inserts; x++) {
	    if(rhash->hash.chk[x].chksum==old_chksum) {
		rhash->duplicates++;
	    } else {
		old_chksum = rhash->hash.chk[x].chksum;
		if(hash_offset) {
		    rhash->hash.chk[x - rhash->duplicates].chksum = old_chksum;
		    rhash->hash.chk[x - rhash->duplicates].offset = 
			rhash->hash.chk[x].offset;
		}
	    }
	}
	rhash->inserts -= rhash->duplicates;
	if((rhash->hash.chk = (chksum_ent *)realloc(rhash->hash.chk, 
	    rhash->inserts * sizeof(chksum_ent)))==NULL) {
	    perror("hmm, weird, realloc failed\n");
	    exit(EXIT_FAILURE);
	}
	rhash->hr_size = rhash->inserts;
	v1printf("hash is %lu bytes\n", rhash->hr_size * sizeof(chksum_ent));
    } else if(rhash->type & RH_RSORT_HASH) {
	v1printf("inserts=%lu, hr_size=%lu\n", rhash->inserts, rhash->hr_size);
	qsort(rhash->hash.chk, rhash->inserts, sizeof(chksum_ent), 
	    cmp_chksum_ent);
	old_chksum = rhash->hash.chk[0].chksum;
	rhash->duplicates=0;
	for(x=1; x < rhash->inserts; x++) {
	    if(rhash->hash.chk[x].chksum==old_chksum) {
		rhash->duplicates++;
	    } else {
		old_chksum = rhash->hash.chk[x].chksum;
		if(rhash->duplicates) {
		    rhash->hash.chk[x - rhash->duplicates].chksum = old_chksum;
		    rhash->hash.chk[x - rhash->duplicates].offset = 
			rhash->hash.chk[x].offset;
		}
	    }
	}
	rhash->inserts -= rhash->duplicates;
	if((rhash->hash.chk = (chksum_ent *)realloc(rhash->hash.chk, 
	    rhash->inserts * sizeof(chksum_ent)))==NULL) {
	    perror("hmm, weird, realloc failed\n");
	    exit(EXIT_FAILURE);
	}
	rhash->hr_size = rhash->inserts;
	v1printf("hash is %lu bytes\n", rhash->hr_size * sizeof(chksum_ent));
    }
    rhash->flags |= RH_SORTED;
    return 0;
}


signed int
RHash_cleanse(RefHash *rhash)
{
    unsigned long x=0, hash_offset=0, y=0, shift=0;
    assert(rhash->inserts);
    if(rhash->type & RH_SORT_HASH) {
	assert(rhash->flags & RH_SORTED);
	v1printf("inserts=%lu, hr_size=%lu\n", rhash->inserts, rhash->hr_size);
//	qsort(rhash->hash.chk, rhash->inserts, sizeof(chksum_ent), 
//	    cmp_chksum_ent);
	rhash->duplicates=0;
	for(x=1; x < rhash->inserts; x++) {
	    if(rhash->hash.chk[x].offset==0) {
		rhash->duplicates++;
	    } else {
		if(hash_offset) {
		    rhash->hash.chk[x - rhash->duplicates].chksum = 
			rhash->hash.chk[x].chksum;
		    rhash->hash.chk[x - rhash->duplicates].offset = 
			rhash->hash.chk[x].offset;
		}
	    }
	}
	rhash->inserts -= rhash->duplicates;
	v1printf("hash was size %lu; %lu bytes, cleansing %lu entries\n",
	    rhash->hr_size, rhash->hr_size * sizeof(chksum_ent), 
	    rhash->duplicates);
	v1printf("hash is now size %lu; %lu bytes\n",
	    rhash->inserts, rhash->inserts * sizeof(chksum_ent));
	if(rhash->inserts) {
	    if((rhash->hash.chk = (chksum_ent *)realloc(rhash->hash.chk, 
		rhash->inserts * sizeof(chksum_ent)))==NULL) {
		perror("hmm, weird, realloc failed\n");
		exit(EXIT_FAILURE);
	    }
	} else {
	    v1printf("no valid entries left, free'ing\n");
	    free(rhash->hash.chk);
	    rhash->hash.chk = NULL;
	}
	rhash->hr_size = rhash->inserts;
    } else if(rhash->type & RH_RSORT_HASH) {
	assert(rhash->flags & RH_SORTED);
	v1printf("inserts=%lu, hr_size=%lu\n", rhash->inserts, rhash->hr_size);
	qsort(rhash->hash.chk, rhash->inserts, sizeof(chksum_ent), 
	    cmp_chksum_ent);
	rhash->duplicates=0;
	if(rhash->hash.chk[0].offset == 0) {
	    rhash->duplicates++;
	}
	for(x=1; x < rhash->inserts; x++) {
	    if(rhash->hash.chk[x].offset==0) {
		rhash->duplicates++;
	    } else {
		if(rhash->duplicates) {
		    rhash->hash.chk[x - rhash->duplicates].chksum = 
			rhash->hash.chk[x].chksum;
		    rhash->hash.chk[x - rhash->duplicates].offset = 
			rhash->hash.chk[x].offset;
		}
	    }
	}
	rhash->inserts -= rhash->duplicates;
	v1printf("hash was %lu bytes, cleansing %lu of %lu\n hash is now %lu bytes\n",
	    rhash->hr_size * sizeof(chksum_ent), rhash->duplicates, 
	    rhash->hr_size, rhash->inserts * sizeof(chksum_ent));
	if(rhash->inserts) {
	    if((rhash->hash.chk = (chksum_ent *)realloc(rhash->hash.chk, 
		rhash->inserts * sizeof(chksum_ent)))==NULL) {
		perror("hmm, weird, realloc failed\n");
		exit(EXIT_FAILURE);
	    }
	} else {
	    v1printf("no valid entries left, freeing\n");
	    free(rhash->hash.chk);
	    rhash->hash.chk = NULL;
	}
	rhash->hr_size = rhash->inserts;
    } else if (rhash->type & RH_BUCKET_HASH) {
	for(x=0; x < rhash->hr_size; x++) {
	    if(rhash->hash.bucket.depth[x] > 0) {
		shift=0;
		for(y=0; y < rhash->hash.bucket.depth[x]; y++) {
		    if(rhash->hash.bucket.offset[x][y]==0) {
			shift++;
		    } else if(shift) {
			rhash->hash.bucket.offset[x][y - shift] = 
			    rhash->hash.bucket.offset[x][y];
			rhash->hash.bucket.chksum[x][y - shift] = 
			    rhash->hash.bucket.chksum[x][y];
		    }
		}
		rhash->hash.bucket.depth[x] -= shift;
		if(rhash->hash.bucket.depth[x]==0) {
		    free(rhash->hash.bucket.chksum[x]);
		    free(rhash->hash.bucket.offset[x]);
		    rhash->hash.bucket.chksum[x] = NULL;
		    rhash->hash.bucket.offset[x] = NULL;
		    continue;
		}
		if((rhash->hash.bucket.chksum[x] = (unsigned short *) realloc(
		    rhash->hash.bucket.chksum[x], sizeof(unsigned short) * 
		    rhash->hash.bucket.depth[x])) == NULL || 
		    (rhash->hash.bucket.offset[x] = (off_u64 *) realloc(
		    rhash->hash.bucket.offset[x], sizeof(off_u64) * 
		    rhash->hash.bucket.depth[x])) == NULL) {
		    perror("shite, realloc failed.\n");
		    abort();
		}
	    }
	}
    }
    rhash->flags |= RH_FINALIZED;
    return 0;
}

void
print_RefHash_stats(RefHash *rhash) {
    unsigned long x, matched=0;
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
    if(rhash->type & RH_RSORT_HASH) {
	for(x=0; x < rhash->hr_size; x++) {
	    if(rhash->hash.chk[x].offset) {
		matched++;
	    }
	}
	v1printf("hash stats: matched entries(%lu), percentage(%f%%)\n", 
	    matched, ((float)matched/rhash->inserts)*100);
    }
    v1printf("hash stats: seed_len(%u), sample_rate(%u)\n", rhash->seed_len,
	rhash->sample_rate);
}
