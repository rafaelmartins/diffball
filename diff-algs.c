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
#include "diff-algs.h"
#include "primes.h"
#include "defs.h"
#include "bit-functions.h"

/* this is largely based on the algorithms detailed in randal burn's various papers.
   Obviously credit for the alg's go to him, although I'm the one who gets the dubious
   credit for bugs in the implementation of said algorithms... */

signed int 
OneHalfPassCorrecting(CommandBuffer *buffer, RefHash *rhash, cfile *ver_cfh)
{
    off_u64 ver_len, ref_len;
    unsigned long x, len;
    unsigned long no_match=0, bad_match=0, good_match=0;
    off_u64 vc, va, vs, vm, rm, hash_offset;
    unsigned int const rbuff_size = 4096, vbuff_size = 4096;
    unsigned char rbuff[rbuff_size], vbuff[vbuff_size];
    off_u64 rbuff_start=0, vbuff_start=0, rbuff_end=0, vbuff_end=0;
    ADLER32_SEED_CTX ads;

    init_adler32_seed(&ads, rhash->seed_len, 1);
    ref_len = cfile_len(rhash->ref_cfh);    
    ver_len = cfile_len(ver_cfh);
    
    va=vs =vc =0;
    vbuff_start = cseek(ver_cfh, 0, CSEEK_FSTART);
    //vbuff_start=0;
    vbuff_end=cread(ver_cfh, vbuff, MIN(vbuff_size, ver_len));
    v3printf("vbuff_start(%lu), vbuff_end(%lu)\n", vbuff_start, vbuff_end);
    while(vc + rhash->seed_len < ver_len) {
	if(vc + rhash->seed_len > vbuff_start + vbuff_end) {
	    v3printf("full refresh of vbuff at vbuff_start(%lu), vc(%lu), fstart(%lu), abs(%lu)\n", 
		vbuff_start, vc, ctell(ver_cfh, CSEEK_FSTART),
		ctell(ver_cfh, CSEEK_ABS));
	    //if(vc > vbuff_start + vbuff_end) {
		vbuff_start = cseek(ver_cfh, vc, CSEEK_FSTART);
		vbuff_end = cread(ver_cfh, vbuff, MIN(ver_len - vbuff_start,vbuff_size));
		v3printf("setting vbuff_start(%lu), vbuff_end(%lu), fstart(%lu)\n", 
		vbuff_start, vbuff_end, ctell(ver_cfh, CSEEK_FSTART));
	    /*} else {
		x = vbuff_size - (vc - vbuff_start);
		memmove(vbuff, vbuff + vbuff_size -x, x);
	    }*/	
	} else if (vc < vbuff_start) {
	    v3printf("partial refresh of vbuff at vbuff_start(%lu), vc(%lu), fstart(%lu), abs(%lu)\n", 
		vbuff_start, vc,ctell(ver_cfh, CSEEK_FSTART),
		    ctell(ver_cfh, CSEEK_ABS));
	    vbuff_start = cseek(ver_cfh, vc, CSEEK_FSTART);
	    vbuff_end = cread(ver_cfh, vbuff, MIN(ver_len - vbuff_start, vbuff_size));
	    v3printf("setting vbuff_start(%lu), vbuff_end(%lu), fstart(%lu)\n", 
		vbuff_start, vbuff_end, ctell(ver_cfh, CSEEK_FSTART));
	}
	//assert(ctell(ver_cfh, CSEEK_FSTART)==vbuff_start + vbuff_end);
	if(va -vc >= rhash->seed_len) {
	    update_adler32_seed(&ads, vbuff + vc - vbuff_start, rhash->seed_len);
	} else {
	    update_adler32_seed(&ads, vbuff + (va - vbuff_start), vc + rhash->seed_len -va);
	}
	va = vc + rhash->seed_len;
//	index = hash_it(rhash, &ads);
	hash_offset = lookup_offset(rhash, &ads);
	if(hash_offset) {
//	    v1printf("possible match for vc(%lu):", vc);
	    if(hash_offset != cseek(rhash->ref_cfh, 
		hash_offset, CSEEK_FSTART)) {

		perror("cseek error for ref\n");
		v3printf("ctell(%lu), wanted(%lu)\n", ctell(rhash->ref_cfh, CSEEK_FSTART), 
		   hash_offset);
		abort();
	    } else {
		rbuff_start = hash_offset;
		rbuff_end = cread(rhash->ref_cfh, rbuff, rbuff_size);
	    }	
	    if(memcmp(rbuff, vbuff + vc - vbuff_start, rhash->seed_len)!=0){
		if(rhash->type & (RH_RMOD_HASH | RH_CMOD_HASH)) {
		    v2printf("bad match: vc(%lu), chk(%lx):i(%lu) chk(%lx):off(%lu)\n",
			vc + cfile_start_offset(ver_cfh),
			get_checksum(&ads),
			(get_checksum(&ads) % rhash->hr_size), 
			rhash->hash.chk[get_checksum(&ads) % 
			    rhash->hr_size].chksum, 
			rhash->hash.chk[get_checksum(&ads) % 
			    rhash->hr_size].offset);
		}
		bad_match++;
		vc++;
		continue;
	    }
	    good_match++;
	    x=0;
	    vm = vc;
	    rm = hash_offset;
	    /*backwards matching*/
	    if(vm-vbuff_start==0) {
		vbuff_start= cseek(ver_cfh, (vbuff_size > vbuff_start ? 0 : 
		    vbuff_start - vbuff_size), CSEEK_FSTART);
		vbuff_end=cread(ver_cfh, vbuff, vbuff_size);
		v3printf("back match moved vbuff to start(%lu), end(%lu)\n",
		    vbuff_start, vbuff_end);
	    }
	    if(rm-rbuff_start==0) {
		rbuff_start= cseek(rhash->ref_cfh, (rbuff_size > 
		    rbuff_start ? 0 : rbuff_start - rbuff_size), CSEEK_FSTART);
		rbuff_end=cread(rhash->ref_cfh, rbuff, rbuff_size);
		v3printf("back match moved rbuff to start(%lu), end(%lu)\n",
		    rbuff_start, rbuff_end);
	    }
	    while(vm > 0 && rm > 0 && vbuff[vm -vbuff_start-1] == 
		rbuff[rm -rbuff_start -1]) {
		vm--;
		rm--;
		if(vm-vbuff_start==0) {
		    vbuff_start= cseek(ver_cfh, (vbuff_size > 
			vbuff_start ? 0 : vbuff_start - vbuff_size), 
			CSEEK_FSTART);
		    vbuff_end=cread(ver_cfh, vbuff, vbuff_size);
		    v3printf("back match moved vbuff to start(%lu), end(%lu)\n",
			vbuff_start, vbuff_end);
		}
		if(rm-rbuff_start==0) {
		    rbuff_start= cseek(rhash->ref_cfh, (rbuff_size > 
			rbuff_start ? 0 : rbuff_start - rbuff_size), 
			    CSEEK_FSTART);
		    rbuff_end=cread(rhash->ref_cfh, rbuff, rbuff_size);
		    v3printf("back match moved rbuff to start(%lu), end(%lu)\n",
		        rbuff_start, rbuff_end);
		}
	    }
	    len=(vc -vm) + rhash->seed_len;

	    if(vm + len >= vbuff_start + vbuff_size) {
		vbuff_start=cseek(ver_cfh, vm+len , CSEEK_FSTART);
		vbuff_end=cread(ver_cfh, vbuff, MIN(vbuff_size, ver_len - 
		    vbuff_start));
		v3printf("forw match moved vbuff to start(%lu), end(%lu)\n",
		    vbuff_start, vbuff_end);
	    }
	    if(rm + len >= rbuff_start + rbuff_size) {
		rbuff_start=cseek(rhash->ref_cfh, rm + len, CSEEK_FSTART);
	    	rbuff_end=cread(rhash->ref_cfh, rbuff, MIN(rbuff_size, 
		    ref_len - rbuff_start));
		v3printf("forw match moved rbuff to start(%lu), end(%lu)\n",
		    rbuff_start, rbuff_end);
	    }
	    while(rm + len < ref_len && vm + len < ver_len &&
		rbuff[rm + len - rbuff_start] == vbuff[vm + len - 
		vbuff_start]) {
		len++;
		if(vm + len -vbuff_start==vbuff_size) {
		    vbuff_start += vbuff_end;
		    vbuff_end=cread(ver_cfh, vbuff, MIN(vbuff_size, ver_len - 
			vbuff_start));
		    	v3printf("forw match moved rbuff to start(%lu), end(%lu)\n",
		    	    vbuff_start, vbuff_end);
		}
		if(rm + len -rbuff_start==rbuff_size) {
		    rbuff_start += rbuff_end;
		    rbuff_end=cread(rhash->ref_cfh, rbuff, MIN(rbuff_size,
			ref_len -rbuff_start));
		    v3printf("forw match moved rbuff to start(%lu), end(%lu)\n",
		    	rbuff_start, rbuff_end);
		}
	    }

	    if (vs <= vm) {
		if (vs < vm) {
		    v2printf("\tadding vstart(%lu), len(%lu), vend(%lu): (vs < vm)\n",
			cfile_start_offset(ver_cfh) + vs, vm-vs, 
			cfile_start_offset(ver_cfh) + vm);
		    DCB_add_add(buffer, cfile_start_offset(ver_cfh) + vs, 
			vm -vs);
//		    DCBufferAddCmd(buffer, DC_ADD, 
//			cfile_start_offset(ver_cfh) + vs, vm - vs);
		}
		v2printf("    copying src_offset(%lu), ver_offset(%lu), len(%lu), ver_end(%lu)\n", 
		    cfile_start_offset(rhash->ref_cfh) + rm, 
		    cfile_start_offset(ver_cfh) + vm, len, 
		    cfile_start_offset(ver_cfh) + vm + len);
		DCB_add_copy(buffer, cfile_start_offset(rhash->ref_cfh) +rm, 
		    cfile_start_offset(ver_cfh) + vm, len);
//		DCBufferAddCmd(buffer, DC_COPY, 
//		    cfile_start_offset(rhash->ref_cfh) + rm, len);
	    } else {
		v2printf("    truncating(%lu) bytes: (vm < vs)\n", vs - vm);
		assert(vs -vm < cfile_len(ver_cfh));
		DCB_truncate(buffer, vs - vm);
//		DCBufferTruncate(buffer, vs - vm);
		v2printf("    replacement copy: offset(%lu), len(%lu)\n", 
		    cfile_start_offset(rhash->ref_cfh) + rm, len);
		DCB_add_copy(buffer, cfile_start_offset(rhash->ref_cfh) + rm, 
		    cfile_start_offset(ver_cfh) + vm, len);
//		DCBufferAddCmd(buffer, DC_COPY, 
//		    cfile_start_offset(rhash->ref_cfh) + rm, len);
	    }
	    vs = vm + len;
	    vc = vs -1;
	} else {
	    no_match++;
	}
	vc++;
    }
    if (vs != ver_len) {
	DCB_add_add(buffer, cfile_start_offset(ver_cfh) + vs, ver_len - vs);
//    	DCBufferAddCmd(buffer, DC_ADD, cfile_start_offset(ver_cfh) + vs, 
//	    ver_len - vs);
    }
    free_adler32_seed(&ads);
//    if(bad_match)
//	v1printf("bad_matches(%lu)\n", bad_match);
    return 0;
}


signed int
MultiPassAlg(CommandBuffer *buff, cfile *ref_cfh, cfile *ver_cfh,
    unsigned long max_hash_size)
{
    RefHash rhash;
    cfile ver_window;
    unsigned long hash_size=0, sample_rate=1;
    unsigned long int seed_len;
    unsigned long gap_req;
    unsigned long gap_total_len;
    unsigned char first_run=0;
    DCLoc dc;
    assert(buff->DCBtype & DCBUFFER_LLMATCHES_TYPE);
    DCB_insert(buff);
    v1printf("multipass, hash_size(%lu)\n", hash_size);
    if(buff->DCB.llm.main_head == NULL) {
	seed_len = 512;
	first_run=1;
    } else {
	seed_len = 128;
    }
    for(/*seed_len = 512*/; seed_len >=16; seed_len /= 2) {
	gap_req = seed_len;// * MULTIPASS_GAP_KLUDGE;
	v1printf("\nseed size(%lu)...\n\n", seed_len);
	gap_total_len = 0;
	DCBufferReset(buff);
#ifdef DEBUG_DCBUFFER
	    assert(DCB_test_llm_main(buff));
#endif
	if(!first_run) {
	    while(DCB_get_next_gap(buff, gap_req, &dc)) {
		assert(dc.len <= buff->ver_size);
		v2printf("gap at %lu:%lu size %lu\n", dc.offset, dc.offset + 
		    dc.len, dc.len);
		gap_total_len += dc.len;
	    }
	    if(gap_total_len == 0) {
		v1printf("not worth taking this pass, skipping to next.\n");
#ifdef DEBUG_DCBUFFER
		assert(DCB_test_llm_main(buff));
#endif
		continue;
	    }
	    hash_size= max_hash_size;
	    //hash_size = MIN(max_hash_size, gap_total_len);
	    sample_rate = COMPUTE_SAMPLE_RATE(hash_size, gap_total_len);
	    v1printf("using hash_size(%lu), sample_rate(%lu)\n", 
		hash_size, sample_rate);
	    init_RefHash(&rhash, ref_cfh, seed_len, sample_rate, 
		hash_size, RH_RBUCKET_HASH);
	    DCBufferReset(buff);
	    v1printf("building hash array out of total_gap(%lu)\n",
		gap_total_len);
	    while(DCB_get_next_gap(buff, gap_req, &dc)) {
		RHash_insert_block(&rhash, ver_cfh, dc.offset, dc.len + 
		    dc.offset);
	    }
	    RHash_sort(&rhash);
	    v1printf("looking for matches in reference file\n");
	    RHash_find_matches(&rhash, ref_cfh);
	    v1printf("cleansing hash, to speed bsearch's\n");
	    RHash_cleanse(&rhash);
	    print_RefHash_stats(&rhash);
	    v1printf("beginning gap processing...\n");
	    DCBufferReset(buff);
	    while(DCB_get_next_gap(buff, gap_req, &dc)) {
		v2printf("handling gap %lu:%lu, size %lu\n", dc.offset, 
		    dc.offset + dc.len, dc.len);
		copen(&ver_window, ver_cfh->raw_fh, dc.offset, dc.len + 
		    dc.offset, NO_COMPRESSOR, CFILE_RONLY);
	        DCB_llm_init_buff(buff, 128);
	        OneHalfPassCorrecting(buff, &rhash, &ver_window);
	        DCB_insert(buff);
	        cclose(&ver_window);
	    }
	} else {
	    first_run=0;
	    DCBufferReset(buff);
	    v1printf("first run\n");
	    hash_size = MIN(max_hash_size, cfile_len(ref_cfh));
	    sample_rate = COMPUTE_SAMPLE_RATE(hash_size, cfile_len(ref_cfh));
	    v1printf("using hash_size(%lu), sample_rate(%lu)\n", 
		hash_size, sample_rate);
	    init_RefHash(&rhash, ref_cfh, seed_len, sample_rate, 
		hash_size, RH_BUCKET_HASH);
	    RHash_insert_block(&rhash, ref_cfh, 0, cfile_len(ref_cfh));
	    print_RefHash_stats(&rhash);
	    v1printf("making initial run...\n");
	    DCB_llm_init_buff(buff, 128);
	    OneHalfPassCorrecting(buff, &rhash, ver_cfh);
	    DCB_insert(buff);
	}
	RHash_sort(&rhash);

#ifdef DEBUG_DCBUFFER
	assert(DCB_test_llm_main(buff));
#endif
	free_RefHash(&rhash);
    }
    return 0;
}
