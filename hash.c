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
inline unsigned long
get_offset(RefHash *rhash, unsigned long index)
{
    chksum_ent ent, *p;
    if(rhash->type & RH_MOD_HASH)
	return (rhash->hash.mod[index]);
    ent.chksum = index;
    p = bsearch(&ent, rhash->hash.chk, rhash->hr_size, sizeof(chksum_ent), 
	cmp_chksum_ent);
    if(p==NULL)
	return 0;
    return p->offset;
}

inline unsigned long 
hash_it(RefHash *rhash, ADLER32_SEED_CTX *ads)
{
    if(rhash->type & RH_MOD_HASH)
	return (get_checksum(ads) % rhash->hr_size);
    return get_checksum(ads);
}

signed int 
free_RefHash(RefHash *rhash)
{
    v2printf("free_RefHash\n");
    if(rhash->type & RH_MOD_HASH)
	free(rhash->hash.mod);
    else 
	free(rhash->hash.chk);
    rhash->ref_cfh = NULL;
    rhash->seed_len = rhash->hr_size = rhash->sample_rate = 
	rhash->inserts = rhash->duplicates = 0;
    return 0;
}

signed int 
init_RefHash(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, 
    unsigned int sample_rate, unsigned long hr_size, unsigned int hash_type)
{
    unsigned long index, old_chksum;
    off_u64 hash_offset = 0;
    assert(seed_len);
    unsigned int const rbuff_size = 16 * 4096;
    unsigned char rbuff[rbuff_size];
    unsigned long rbuff_start=0, rbuff_end=0;
    off_u64 ref_len, x;
    unsigned int missed;

    rhash->type = hash_type;
    ADLER32_SEED_CTX ads;
    PRIME_CTX pctx;
#ifdef DEBUG_HASH
    unsigned char *test_buff;
    if((test_buff=(unsigned char*)malloc(seed_len))==NULL) {
	abort();
    }
    rhash->bad_duplicates=0;
#endif

    v2printf("init_RefHash\n");
    init_primes(&pctx);
    rhash->hr_size = get_nearest_prime(&pctx, hr_size);
    rhash->ref_cfh = ref_cfh;
    rhash->seed_len = seed_len;
    ref_len = cfile_len(ref_cfh);
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
    } else {
	if((rhash->hash.chk = (chksum_ent *)malloc(sizeof(chksum_ent) * 
	    rhash->hr_size))==NULL) {
	    perror("shite, couldn't alloc needed memory for ref hash\n");
	    exit(EXIT_FAILURE);
	}
    }
    init_adler32_seed(&ads, seed_len, 1);
    rbuff_start = cseek(ref_cfh, 0, CSEEK_FSTART);
    rbuff_end=cread(ref_cfh, rbuff, rbuff_size);
    //v2printf("rbuff_end(%lu)\n", rbuff_end);

    update_adler32_seed(&ads, rbuff, seed_len);
    missed=0;
/* kludge I realize for the init, but neh, need something better.
   the specific issue is that mod_hash can track if it's inserted in a sample 
   window or not (and adjust accordingly)- this results in a better cross 
   section, not always starting on the same offset.
   sort_hash cannot unfortunately, so need a kludge of some sort to try and
   vary up the start position. */
    for(x=seed_len * (sample_rate > 1 ? 1.5 : 1); 
	x < ref_len - seed_len; x++) {
	if(x - rbuff_start >= rbuff_size) {
	    rbuff_start += rbuff_end;
#ifdef DEBUG_HASH
	    cseek(ref_cfh, rbuff_start, CSEEK_FSTART);
#endif
	    rbuff_end   = cread(ref_cfh, rbuff, 
		MIN(ref_len - rbuff_start, rbuff_size));
	}
	update_adler32_seed(&ads, rbuff + (x - rbuff_start), 1);
	index=hash_it(rhash, &ads);

	if(rhash->type & RH_MOD_HASH) {
	    /*note this has the ability to overwrite offset 0...
	      but thats alright, cause a correcting alg if a match at offset1, will grab the offset 0 */
	    hash_offset = get_offset(rhash, index);
	    if(hash_offset==0) {
		rhash->inserts++;
		rhash->hash.mod[index] =x - seed_len + 1;
		if(sample_rate > 1 && missed < sample_rate) {
		    x+= sample_rate - missed;
		}
		//x += (missed >= sample_rate ? 0 : sample_rate - missed);
		missed=0;
	    } else {
		rhash->duplicates++;
		missed++;
#ifdef DEBUG_HASH
		cseek(ref_cfh, hash_offset, CSEEK_FSTART);
		cread(ref_cfh, test_buff, seed_len);
		if((memcmp(test_buff, ads.seed_chars + ads.tail, seed_len - 
		    ads.tail)!=0) &&
		    (memcmp(test_buff + ads.tail, ads.seed_chars, 
		    ads.tail)!=0)) {
			rhash->bad_duplicates++;
		}
#endif
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
	    rhash->hash.chk[rhash->inserts].chksum = index;
	    rhash->hash.chk[rhash->inserts].offset = x - seed_len + 1;
	    rhash->inserts++;
	    if(sample_rate > 1)
		x += sample_rate;
	}
    }
    if(rhash->type & RH_SORT_HASH) {
	v1printf("inserts=%lu, hr_size=%lu\n", rhash->inserts, rhash->hr_size);
	qsort(rhash->hash.chk, rhash->inserts, sizeof(chksum_ent), 
	    cmp_chksum_ent);
	old_chksum = rhash->hash.chk[0].chksum;
	rhash->duplicates=0;
//	v1printf("chksums dump\n");
	for(x=1; x < rhash->inserts; x++) {
//	    v1printf("%10.10lu, %10.10lu\n", rhash->hash.chk[x].chksum,
//		rhash->hash.chk[x].offset);
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
    }
    free_primes(&pctx);
    free_adler32_seed(&ads);
#ifdef DEBUG_HASH
    free(test_buff);
#endif
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
    v1printf("hash stats: seed_len(%u)\n", rhash->seed_len);
}
