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
lookup_offset(RefHash *rhash, ADLER32_SEED_CTX *ads)
{
    chksum_ent ent;
    unsigned long chksum, index;
    void *p;
    chksum = get_checksum(ads);
    if(rhash->type & RH_MOD_HASH) {
	return (rhash->hash.mod[chksum % rhash->hr_size]);
    } else if (rhash->type & RH_RMOD_HASH) {
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
    }
    return 0;
}

/* this is kind of stupid I realize, but certain hashing methods return 
   different dependant on the hash state. */
inline unsigned long
get_offset(RefHash *rhash, unsigned long index)
{
    chksum_ent ent;
    void *p;
    if(rhash->type & RH_MOD_HASH) {
	return (rhash->hash.mod[index]);
    } else if (rhash->type & RH_RMOD_HASH) {
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

inline unsigned long 
hash_it(RefHash *rhash, ADLER32_SEED_CTX *ads)
{
    if(rhash->type & (RH_MOD_HASH | RH_RMOD_HASH)) {
	return (get_checksum(ads) % rhash->hr_size);
//  could use just a bitmask, although doesn't perform quite as well.
//	return (get_checksum(ads) & rhash->hr_size);
    }
    return get_checksum(ads);
}

signed int 
free_RefHash(RefHash *rhash)
{
    v2printf("free_RefHash\n");
    if((rhash->type & RH_MOD_HASH) && (rhash->hash.mod != NULL))
	free(rhash->hash.mod);
    else if((rhash->type & (RH_SORT_HASH | RH_RSORT_HASH | RH_RMOD_HASH)) && 
	(rhash->hash.chk != NULL))
	free(rhash->hash.chk);
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
    init_primes(&pctx);
    rhash->hr_size = get_nearest_prime(&pctx, hr_size);
    free_primes(&pctx);
    rhash->sample_rate = sample_rate;
    rhash->ref_cfh = ref_cfh;
    rhash->seed_len = seed_len;
    rhash->inserts = rhash->duplicates = 0;
#ifdef DEBUG_HASH
    rhash->bad_duplicates=0;
#endif
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
    } else if(rhash->type & RH_RMOD_HASH) {
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
    }
    return 0;
}

signed int
RHash_insert_block(RefHash *rhash, cfile *ref_cfh, off_u64 ref_start, 
    off_u64 ref_end)
{
    ADLER32_SEED_CTX ads;
    off_u64 hash_offset = 0;
    unsigned long index;
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

    for(cfw->pos += rhash->seed_len; cfw->offset + cfw->pos < ref_end; 
	cfw->pos++) {
	if(cfw->pos >= cfw->end) {
	    cfw = next_page(ref_cfh);
	    if(cfw->end==0) {
		abort();
	    }
	}

	if(rhash->type & RH_MOD_HASH) {
	    index=hash_it(rhash, &ads);
	    hash_offset = get_offset(rhash, index);
	    if(hash_offset==0) {
		rhash->inserts++;
		rhash->hash.mod[index] = cfw->offset + cfw->pos - 
		    rhash->seed_len;
		if(rhash->sample_rate > 1 && missed < rhash->sample_rate) {
		    cfw->pos += rhash->sample_rate - missed;
		}
		missed=0;
	    } else {
		rhash->duplicates++;
		missed++;
	    }
	} else if(rhash->type & RH_RMOD_HASH) {
	    index = hash_it(rhash, &ads);
	    if(rhash->hash.chk[index].chksum==0) {
		rhash->inserts++;
		rhash->hash.chk[index].chksum = get_checksum(&ads);
		if(rhash->sample_rate > 1 && missed < rhash->sample_rate) {
		    cfw->pos += rhash->sample_rate - missed;
		}
		missed=0;
	    } else {
		rhash->duplicates++;
		missed++;
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
		cfw->pos += rhash->sample_rate;
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
		cfw->pos += rhash->sample_rate;
	}		
	update_adler32_seed(&ads, cfw->buff + cfw->pos, 1);
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
    if(!(rhash->type & (RH_RSORT_HASH | RH_RMOD_HASH))) {
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
//hash_it(rhash, &ads);
	    if(rhash->hash.chk[index].chksum == chksum) {
		rhash->hash.chk[index].offset = cfw->offset + cfw->pos - 
		    rhash->seed_len;
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
    unsigned long x=0, hash_offset=0;
    assert(rhash->inserts);
    assert(rhash->flags & RH_SORTED);
    if(rhash->type & RH_SORT_HASH) {
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
    v1printf("hash stats: seed_len(%u)\n", rhash->seed_len);
}
