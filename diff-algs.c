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

signed int init_RefHash(struct ref_hash *rhash, struct cfile *ref_cfh, 
	unsigned int seed_len, unsigned long hr_size)
{
unsigned long *hr; //reference hash table.
    unsigned long x, index, len;
    //unsigned long inserts=0, duplicates=0;
    unsigned int const rbuff_size = 1024;
    unsigned char rbuff[rbuff_size];
    unsigned long rbuff_start=0, rbuff_end=0;
    unsigned long ref_len;

    struct adler32_seed ads;
    //unsigned long copies=0, adds=0, truncations=0;
    //unsigned long min_offset_dist = 0, max_offset_dist = 0;
	struct prime_ctx pctx;

	/*if((buffer=(struct CommandBuffer *)malloc(sizeof(struct CommandBuffer)))==NULL) {
		perror("shite, couldn't alloc needed memory.\n");
		exit(EXIT_FAILURE);
	}*/
	init_primes(&pctx);
	rhash->hr_size = get_nearest_prime(&pctx, hr_size);
	printf("given hr_size(%lu), using(%lu)\n", hr_size, rhash->hr_size);
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
    rbuff_end=cread(ref_cfh, rbuff, rbuff_size);
    //printf("rbuff_end(%lu)\n", rbuff_end);
    rbuff_start = 0;
    update_adler32_seed(&ads, rbuff, seed_len);

    for(x=seed_len; x < ref_len - seed_len; x++) {
		if(x - rbuff_start >= rbuff_size) {
			rbuff_start += rbuff_end;
			rbuff_end   = cread(ref_cfh, rbuff, 
				MIN(ref_len - rbuff_start, rbuff_size));
		}
		/*s1 = s1 - last_seed[x % seed_len] +
			rbuff[x - rbuff_start];
		s2 = s2 - (seed_len * last_seed[x % seed_len]) + s1;
		last_seed[x % seed_len] = rbuff[x - rbuff_start];
		*/
		/*for(rm=x; rm < x + seed_len; rm++){
			s1 += rbuff[rm - rbuff_start];
			s2 += s1;
		}*/
		//hr[x - seed_len+1];
		update_adler32_seed(&ads, rbuff + (x - rbuff_start), 1);
		index=hash_it(ads, rhash->hr_size);
		//printf("s1(%lu), s2(%lu) hashed(%lu), off(%lu)\n", ads.s1,ads.s2, index, 
		//	x - seed_len + 1);
		/*note this has the ability to overwrite offset 0...
	  but thats alright, cause a correcting alg if a match at offset1, will grab the offset 0 */
		if(rhash->hash[index]==0) {
	    	rhash->inserts++;
	    	rhash->hash[index] =x - seed_len + 1;
		} else {
			rhash->duplicates++;
	    }
    }
}

signed int OneHalfPassCorrecting(struct CommandBuffer *buffer, 
	struct ref_hash *rhash, struct cfile *ver_cfh)
{
    unsigned long ver_len, ref_len;
    unsigned long x, index, len;
	unsigned long no_match=0, bad_match=0, good_match=0;
    unsigned long vc, va, vs, vm, rm;
    unsigned int const rbuff_size = 512, vbuff_size = 1024;
    unsigned char rbuff[rbuff_size], vbuff[vbuff_size];
    unsigned long rbuff_start=0, vbuff_start=0, rbuff_end=0, vbuff_end=0;
    struct adler32_seed ads;

	init_adler32_seed(&ads, rhash->seed_len, 1);
	ref_len = rhash->ref_cfh->byte_len;    
    ver_len = ver_cfh->byte_len;
    
    va=vs =vc =0;
    cseek(ver_cfh, 0, CSEEK_ABS);
    vbuff_start=0;
    vbuff_end=cread(ver_cfh, vbuff, MIN(vbuff_size, ver_len));
    //printf("vc(%lu), rhash->seed_len(%lu), ver_len(%lu)\n", vc, rhash->seed_len, ver_len);
	while(vc + rhash->seed_len < ver_len) {
		//printf("handling vc(%lu)\n", vc);
		if(vc + rhash->seed_len > vbuff_start + vbuff_size) {
			printf("full refresh of vbuff\n");
			//if(vc > vbuff_start + vbuff_size) {
				vbuff_start = cseek(ver_cfh, vc, CSEEK_ABS);
				vbuff_end = cread(ver_cfh, vbuff, MIN(ver_len - vbuff_start,vbuff_size));
			//} else {
			//	x = vbuff_size - (vc - vbuff_start);
			//	memmove(vbuff, vbuff + vbuff_size -x, x);
				
		} else if (vc < vbuff_start) {
			printf("partial refresh of vbuff\n");
			vbuff_start = cseek(ver_cfh, vc, CSEEK_ABS);
			vbuff_end = cread(ver_cfh, vbuff, MIN(ver_len - vbuff_start, vbuff_size));
		}
		assert(ctell(ver_cfh, CSEEK_ABS)==vbuff_start + vbuff_end);
		if(va -vc >= rhash->seed_len) {
			//printf("flushing loc(%lu)\n", vc - vbuff_start);
			update_adler32_seed(&ads, vbuff + vc - vbuff_start, rhash->seed_len);
		} else {
			//printf("rolling (%u)\n", vc + rhash->seed_len -va);
			update_adler32_seed(&ads, vbuff + (va - vbuff_start), vc + rhash->seed_len -va);
		}
		va = vc + rhash->seed_len;
		index = hash_it(ads, rhash->hr_size);
		if(rhash->hash[index]>0) {	
			if(rhash->hash[index] != cseek(rhash->ref_cfh, rhash->hash[index], CSEEK_FSTART)) {
				perror("cseek error for ref\n");
				abort();
			} else {
				rbuff_start = rhash->hash[index];
				rbuff_end = cread(rhash->ref_cfh, rbuff, rbuff_size);
			}	
			if(memcmp(rbuff, vbuff+vc - vbuff_start, rhash->seed_len)!=0){
				//printf("bad collision(%lu).\n", vc);
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
		   			CSEEK_ABS);
		   		vbuff_end=cread(ver_cfh, vbuff, vbuff_size);
		   		//printf("initial ver back cseek(%lu)\n", vbuff_start);
		   	}
		   	if(rm-rbuff_start==0) {
		   		//printf("rm-rbuff_start==0\n");
		   		rbuff_start=
		  			cseek(rhash->ref_cfh, 
		  			(rbuff_size > rbuff_start ? 0 : rbuff_start - rbuff_size),
		  			CSEEK_ABS);
		  		rbuff_end=cread(rhash->ref_cfh, rbuff, rbuff_size);
		  		//printf("initial ref back cseek(%lu)\n", rbuff_start);
			}
		    while(vm > 0 && rm > 0 && 
		    	vbuff[vm -vbuff_start-1]==rbuff[rm -rbuff_start -1]) {
				vm--;
				rm--;
		    	if(vm-vbuff_start==0) {
		    		vbuff_start=
		    			cseek(ver_cfh, (vbuff_size > vbuff_start ? 0 :
		    			vbuff_start - vbuff_size), CSEEK_ABS);
		    		vbuff_end=cread(ver_cfh, vbuff, vbuff_size);
		    		//printf("pushing ver back %u to cseek(%lu)\n", vbuff_size, vbuff_start);
		    	}
		    	if(rm-rbuff_start==0) {
		    		rbuff_start=
		    			cseek(rhash->ref_cfh, (rbuff_size > rbuff_start ? 0 :
		    			rbuff_start - rbuff_size), CSEEK_ABS);
		    		rbuff_end=cread(rhash->ref_cfh, rbuff, rbuff_size);
		    		//printf("pushing ref back %u to cseek(%lu)\n", vbuff_size, vbuff_start);
		    	}
		    }
		    len=(vc -vm) + rhash->seed_len;
		    /*printf("stopped back match, v(%u)==r(%u)\n",vbuff[vm-vbuff_start],
		    rbuff[rm-rbuff_start]);
		    printf("stopped back match, v(%u)!=r(%u)\n",vbuff[vm-vbuff_start-1],
		    rbuff[rm-rbuff_start-1]);
		    printf("vbuff_start(%lu), vm(%lu)\n",vbuff_start, vm,len);
		    printf("rbuff_start(%lu), rm(%lu)\n",rbuff_start, rm,len);
		    */
		    if(vm + len >= vbuff_start + vbuff_size) {
		    	vbuff_start=cseek(ver_cfh, vm+len , CSEEK_ABS);
		    	vbuff_end=cread(ver_cfh, vbuff,
		    		MIN(vbuff_size, ver_len - vbuff_start));
		    	//printf("initial ver forw cseek(%lu)\n", vbuff_start);
		    }
		    if( rm + len >= rbuff_start + rbuff_size) {
		    	rbuff_start=cseek(rhash->ref_cfh, rm + len, CSEEK_ABS);
		    	rbuff_end=cread(rhash->ref_cfh, rbuff,
		    		MIN(rbuff_size, ref_len - rbuff_start));
		    	//printf("initial ref forw cseek(%lu)\n", rbuff_start);
		    }
		    while(rm + len < ref_len && vm + len < ver_len &&
		    	rbuff[rm + len - rbuff_start]
		    	==vbuff[vm + len - vbuff_start]) {
		    	len++;
		    	if(vm + len -vbuff_start==vbuff_size) {
		    		vbuff_start += vbuff_end;
		    		vbuff_end=cread(ver_cfh, vbuff,
		    			MIN(vbuff_size, ver_len -vbuff_start));
		    		//printf("pushing ver forw %u to cseek(%lu)\n", vbuff_size, vbuff_start);
		    	}
		    	if(rm + len -rbuff_start==rbuff_size) {
		    		rbuff_start += rbuff_end;
		    		rbuff_end=cread(rhash->ref_cfh, rbuff,
		    			MIN(rbuff_size, ref_len -rbuff_start));
					//printf("pushing ref forw %u to cseek(%lu)\n", rbuff_size, rbuff_start);
		    	}
		    }
		    //printf("stopped forw match, v(%u)!=r(%u)\n",vbuff[vm-vbuff_start+len],
		    //rbuff[rm-rbuff_start+len]);
		    /*printf("ver_len(%lu), vm(%lu)+len(%lu)==(%lu)\n",ver_len,vm,len,vm+len);
		    printf("ref_len(%lu), rm(%lu)+len(%lu)==(%lu)\n",ref_len,rm,len,rm+len);
		    printf("vbuff_start(%lu), rbuff_start(%lu)\n", vbuff_start, rbuff_start);
	    	printf("good collision(%lu):vstart(%lu), rstart(%lu), len(%lu)\n", vc,vm, rm, len);
	    	*/
	    	if (vs <= vm) {
				if (vs < vm) {
		    		printf("    adding vstart(%lu), len(%lu), vend(%lu): (vs < vm)\n",
						vs, vm-vs, vm);
		    		//DCBufferAddCmd(&buffer, DC_ADD, vs -ver, vm - vs);
		    		DCBufferAddCmd(buffer, DC_ADD, vs, vm - vs);
		    		//adds++;
				}
				printf("    copying offset(%lu), len(%lu)\n", vm, len);
				DCBufferAddCmd(buffer, DC_COPY, rm, len);
		    } else {
				printf("    truncating(%lu) bytes: (vm < vs)\n", vs - vm);
				DCBufferTruncate(buffer, vs - vm);
				printf("    replacement copy: offset(%lu), len(%lu)\n", rm, len);
				//DCBufferAddCmd(&buffer, DC_COPY, rm -ref, len);
				DCBufferAddCmd(buffer, DC_COPY, rm, len);
				//truncations++;
	    	}
	    	//copies++;
	    	vs = vm + len;
	    	vc = vs -1;
		} else {
	    	no_match++;
	    	//printf("no match(%lu)\n", vc);
		}
		vc++;
    }
    if (vs != ver_len) {
    	DCBufferAddCmd(buffer, DC_ADD, vs, ver_len - vs);
    }
    /*printf("version summary:\n");
    printf("no_matches(%f%%)\n", (float)no_match/(float)(no_match+good_match+bad_match)*100);
    printf("bad_matches(%f%%)\n",(float)bad_match/(float)(no_match+bad_match+good_match)*100);
    printf("good_matches(%f%%)\n",(float)good_match/(float)(no_match+bad_match+good_match)*100);
    printf("(%%)commands in buffer, copies(%lu), adds(%lu), truncations(%lu)\n", copies, adds, truncations);
    printf("\n\nflushing command buffer...\n\n\n");
    */
    //DCBufferFlush(&buffer, ver, out_fh);
    //if(encoding_type==USE_GDIFF_ENCODING) {
		//printf("using gdiff encoding...\n");
		//if(gdiffEncodeDCBuffer(&buffer, offset_type, ver, out_fh)) {
		//if(switchingEncodeDCBuffer(buffer, offset_type, ver_cfh, out_cfh)) {
		
		//    printf("wtf? error returned from encoding engine\n");
		//}
    //}
    //return NULL;
    return 0;
}
