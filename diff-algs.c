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
//#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "adler32.h"
#include "diff-algs.h"
#include "bit-functions.h"
//#include "gdiff.h"
//#include "switching.h"
#include "primes.h"

/* this is largely based on the algorithms detailed in randal burn's various papers.
   Obviously credit for the alg's go to him, although I'm the one who gets the dubious
   credit for bugs in the implementation of said algorithms... */
//#define LOOKBACK_SIZE 100000
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define hash_it(ads, tbl_size) (get_checksum(&ads) % tbl_size)

signed int 
free_RefHash(RefHash *rhash)
{
	printf("free_RefHash\n");
	free(rhash->hash);
	rhash->seed_len = rhash->hr_size = rhash->sample_rate = 
		rhash->inserts = rhash->duplicates = 0;
	return 0;
}

signed int 
init_RefHash(RefHash *rhash, cfile *ref_cfh, unsigned int seed_len, 
    unsigned int sample_rate, unsigned long hr_size)
{
	unsigned long x, index;
    //unsigned long inserts=0, duplicates=0;
    unsigned int const rbuff_size = 16 * 4096;
    unsigned char rbuff[rbuff_size];
    unsigned long rbuff_start=0, rbuff_end=0;
    unsigned long ref_len;
    unsigned int missed;

    ADLER32_SEED_CTX ads;
    //unsigned long copies=0, adds=0, truncations=0;
    //unsigned long min_offset_dist = 0, max_offset_dist = 0;
    PRIME_CTX pctx;
    printf("init_RefHash\n");
    init_primes(&pctx);
	rhash->hr_size = get_nearest_prime(&pctx, hr_size);
	rhash->ref_cfh = ref_cfh;
	rhash->seed_len = seed_len;
	ref_len = ref_cfh->byte_len;
	rhash->inserts = rhash->duplicates = 0;
    if((rhash->hash=(unsigned long*)malloc(sizeof(unsigned long)*(rhash->hr_size)))==NULL) {
		perror("Shite.  couldn't allocate needed memory for reference hash table.\n");
		exit(EXIT_FAILURE);
    }
    // init the bugger==0
    for(x=0; x < rhash->hr_size; x++) {
		rhash->hash[x] = 0;
    }
    init_adler32_seed(&ads, seed_len, 1);
    rbuff_start = cseek(ref_cfh, 0, CSEEK_FSTART);
    rbuff_end=cread(ref_cfh, rbuff, rbuff_size);
    //printf("rbuff_end(%lu)\n", rbuff_end);

    update_adler32_seed(&ads, rbuff, seed_len);
    missed=0;
    for(x=seed_len; x < ref_len - seed_len; x++) {
		if(x - rbuff_start >= rbuff_size) {
			rbuff_start += rbuff_end;
			rbuff_end   = cread(ref_cfh, rbuff, 
				MIN(ref_len - rbuff_start, rbuff_size));
		}
		update_adler32_seed(&ads, rbuff + (x - rbuff_start), 1);
		index=hash_it(ads, rhash->hr_size);

		/*note this has the ability to overwrite offset 0...
	  but thats alright, cause a correcting alg if a match at offset1, will grab the offset 0 */
		if(rhash->hash[index]==0) {
		    rhash->inserts++;
		    rhash->hash[index] =x - seed_len + 1;
		    if(sample_rate > 1 && missed < sample_rate) {
			x+= sample_rate - missed;
		    }
		    //x += (missed >= sample_rate ? 0 : sample_rate - missed);
		    missed=0;
		} else {
		    rhash->duplicates++;
		    missed++;
	    }
    }
    free_primes(&pctx);
    free_adler32_seed(&ads);
    return 0;
}

signed int 
OneHalfPassCorrecting(CommandBuffer *buffer, RefHash *rhash, cfile *ver_cfh)
{
    unsigned long ver_len, ref_len;
    unsigned long x, index, len;
	unsigned long no_match=0, bad_match=0, good_match=0;
    unsigned long vc, va, vs, vm, rm;
    unsigned int const rbuff_size = 4096, vbuff_size = 4096;
    unsigned char rbuff[rbuff_size], vbuff[vbuff_size];
    unsigned long rbuff_start=0, vbuff_start=0, rbuff_end=0, vbuff_end=0;
    ADLER32_SEED_CTX ads;

	init_adler32_seed(&ads, rhash->seed_len, 1);
	ref_len = rhash->ref_cfh->byte_len;    
    ver_len = ver_cfh->byte_len;
    
    va=vs =vc =0;
    vbuff_start = cseek(ver_cfh, 0, CSEEK_FSTART);
    //vbuff_start=0;
    vbuff_end=cread(ver_cfh, vbuff, MIN(vbuff_size, ver_len));
	printf("vbuff_start(%lu), vbuff_end(%lu)\n", vbuff_start, vbuff_end);
    //printf("vc(%lu), rhash->seed_len(%lu), ver_len(%lu)\n", vc, rhash->seed_len, ver_len);
	while(vc + rhash->seed_len < ver_len) {
		//printf("handling vc(%lu)\n", vc);
		//printf("raw_fh_start=%lu\n", ver_cfh->raw_fh_start);
		if(vc + rhash->seed_len > vbuff_start + vbuff_end) {
			printf("full refresh of vbuff at vbuff_start(%lu), vc(%lu), fstart(%lu), abs(%lu)\n", 
				vbuff_start, vc, 
				ctell(ver_cfh, CSEEK_FSTART),
				ctell(ver_cfh, CSEEK_ABS));
			//if(vc > vbuff_start + vbuff_end) {
				vbuff_start = cseek(ver_cfh, vc, CSEEK_FSTART);
				vbuff_end = cread(ver_cfh, vbuff, MIN(ver_len - vbuff_start,vbuff_size));
				printf("setting vbuff_start(%lu), vbuff_end(%lu), fstart(%lu)\n", 
				vbuff_start, vbuff_end, ctell(ver_cfh, CSEEK_FSTART));
			/*} else {
				x = vbuff_size - (vc - vbuff_start);
				memmove(vbuff, vbuff + vbuff_size -x, x);
			}*/	
		} else if (vc < vbuff_start) {
			printf("partial refresh of vbuff at vbuff_start(%lu), vc(%lu), fstart(%lu), abs(%lu)\n", 
				vbuff_start, vc,
				ctell(ver_cfh, CSEEK_FSTART),
				ctell(ver_cfh, CSEEK_ABS));
			vbuff_start = cseek(ver_cfh, vc, CSEEK_FSTART);
			vbuff_end = cread(ver_cfh, vbuff, MIN(ver_len - vbuff_start, vbuff_size));
			printf("setting vbuff_start(%lu), vbuff_end(%lu), fstart(%lu)\n", 
				vbuff_start, vbuff_end, ctell(ver_cfh, CSEEK_FSTART));
		}
		//assert(ctell(ver_cfh, CSEEK_FSTART)==vbuff_start + vbuff_end);
		if(va -vc >= rhash->seed_len) {
			update_adler32_seed(&ads, vbuff + vc - vbuff_start, rhash->seed_len);
		} else {
			update_adler32_seed(&ads, vbuff + (va - vbuff_start), vc + rhash->seed_len -va);
		}
		va = vc + rhash->seed_len;
		index = hash_it(ads, rhash->hr_size);
		if(rhash->hash[index]>0) {	
			if(rhash->hash[index] != cseek(rhash->ref_cfh, rhash->hash[index], CSEEK_FSTART)) {
				perror("cseek error for ref\n");
				printf("ctell(%lu), wanted(%lu)\n", ctell(rhash->ref_cfh, CSEEK_FSTART), 
					rhash->hash[index]);
				abort();
			} else {
				rbuff_start = rhash->hash[index];
				rbuff_end = cread(rhash->ref_cfh, rbuff, rbuff_size);
			}	
			if(memcmp(rbuff, vbuff+vc - vbuff_start, rhash->seed_len)!=0){
				bad_match++;
				vc++;
				continue;
		    }
		    good_match++;
		    x=0;
		    vm = vc;
		    rm = rhash->hash[index];
		    /*backwards matching*/
		    if(vm-vbuff_start==0) {
		   		vbuff_start=
		   			cseek(ver_cfh, 
		   			(vbuff_size > vbuff_start ? 0 : vbuff_start - vbuff_size),
		   			CSEEK_FSTART);
		   		vbuff_end=cread(ver_cfh, vbuff, vbuff_size);
		   		printf("back match moved vbuff to start(%lu), end(%lu)\n",
		    			vbuff_start, vbuff_end);
		   	}
		   	if(rm-rbuff_start==0) {
		   		rbuff_start=
		  			cseek(rhash->ref_cfh, 
		  			(rbuff_size > rbuff_start ? 0 : rbuff_start - rbuff_size),
		  			CSEEK_FSTART);
		  		rbuff_end=cread(rhash->ref_cfh, rbuff, rbuff_size);
		  		printf("back match moved rbuff to start(%lu), end(%lu)\n",
		    			rbuff_start, rbuff_end);
			}
		    while(vm > 0 && rm > 0 && 
		    	vbuff[vm -vbuff_start-1]==rbuff[rm -rbuff_start -1]) {
				vm--;
				rm--;
		    	if(vm-vbuff_start==0) {
		    		vbuff_start=
		    			cseek(ver_cfh, (vbuff_size > vbuff_start ? 0 :
		    			vbuff_start - vbuff_size), CSEEK_FSTART);
		    		vbuff_end=cread(ver_cfh, vbuff, vbuff_size);
		    		printf("back match moved vbuff to start(%lu), end(%lu)\n",
		    			vbuff_start, vbuff_end);
		    	}
		    	if(rm-rbuff_start==0) {
		    		rbuff_start=
		    			cseek(rhash->ref_cfh, (rbuff_size > rbuff_start ? 0 :
		    			rbuff_start - rbuff_size), CSEEK_FSTART);
		    		rbuff_end=cread(rhash->ref_cfh, rbuff, rbuff_size);
		    		printf("back match moved rbuff to start(%lu), end(%lu)\n",
		    			rbuff_start, rbuff_end);
		    	}
		    }
		    len=(vc -vm) + rhash->seed_len;

		    if(vm + len >= vbuff_start + vbuff_size) {
		    	vbuff_start=cseek(ver_cfh, vm+len , CSEEK_FSTART);
		    	vbuff_end=cread(ver_cfh, vbuff,
		    		MIN(vbuff_size, ver_len - vbuff_start));
		    	printf("forw match moved vbuff to start(%lu), end(%lu)\n",
		    		vbuff_start, vbuff_end);
		    }
		    if( rm + len >= rbuff_start + rbuff_size) {
		    	rbuff_start=cseek(rhash->ref_cfh, rm + len, CSEEK_FSTART);
		    	rbuff_end=cread(rhash->ref_cfh, rbuff,
		    		MIN(rbuff_size, ref_len - rbuff_start));
		    		printf("forw match moved rbuff to start(%lu), end(%lu)\n",
		    			rbuff_start, rbuff_end);
		    }
		    while(rm + len < ref_len && vm + len < ver_len &&
		    	rbuff[rm + len - rbuff_start]
		    	==vbuff[vm + len - vbuff_start]) {
		    	len++;
		    	if(vm + len -vbuff_start==vbuff_size) {
		    		vbuff_start += vbuff_end;
		    		vbuff_end=cread(ver_cfh, vbuff,
		    			MIN(vbuff_size, ver_len -vbuff_start));
		    		printf("forw match moved rbuff to start(%lu), end(%lu)\n",
		    			vbuff_start, vbuff_end);
		    	}
		    	if(rm + len -rbuff_start==rbuff_size) {
		    		rbuff_start += rbuff_end;
		    		rbuff_end=cread(rhash->ref_cfh, rbuff,
		    			MIN(rbuff_size, ref_len -rbuff_start));
		    		printf("forw match moved rbuff to start(%lu), end(%lu)\n",
		    			rbuff_start, rbuff_end);
		    	}
		    }

	    	if (vs <= vm) {
				if (vs < vm) {
		    		printf("    adding vstart(%lu), len(%lu), vend(%lu): (vs < vm)\n",
						ver_cfh->raw_fh_start + vs, vm-vs, vm);
		    		DCBufferAddCmd(buffer, DC_ADD, ver_cfh->raw_fh_start + vs, vm - vs);
				}
				printf("    copying offset(%lu), len(%lu)\n", 
					ver_cfh->raw_fh_start + vm, len);
				DCBufferAddCmd(buffer, DC_COPY, rhash->ref_cfh->raw_fh_start + rm, len);
		    } else {
				printf("    truncating(%lu) bytes: (vm < vs)\n", vs - vm);
				DCBufferTruncate(buffer, vs - vm);
				printf("    replacement copy: offset(%lu), len(%lu)\n", 
					rhash->ref_cfh->raw_fh_start + rm, len);
				DCBufferAddCmd(buffer, DC_COPY, rhash->ref_cfh->raw_fh_start + rm, len);
	    	}
	    	vs = vm + len;
	    	vc = vs -1;
		} else {
	    	no_match++;
		}
		vc++;
    }
    if (vs != ver_len) {
    	DCBufferAddCmd(buffer, DC_ADD, ver_cfh->raw_fh_start + vs, ver_len - vs);
    }
    free_adler32_seed(&ads);
    return 0;
}
