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
    unsigned long ver_len, ref_len;
    unsigned long x, index, len;
    unsigned long no_match=0, bad_match=0, good_match=0;
    off_u64 vc, va, vs, vm, rm, hash_offset;
    unsigned int const rbuff_size = 4096, vbuff_size = 4096;
    unsigned char rbuff[rbuff_size], vbuff[vbuff_size];
    unsigned long rbuff_start=0, vbuff_start=0, rbuff_end=0, vbuff_end=0;
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
	index = hash_it(rhash, &ads);
	hash_offset = get_offset(rhash, index);
	if(hash_offset) {
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
	    if(memcmp(rbuff, vbuff+vc - vbuff_start, rhash->seed_len)!=0){
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
    return 0;
}


signed int
MultiPassAlg(CommandBuffer *buff, cfile *src_cfh, cfile *ver_cfh,
    unsigned long hash_size)
{
    RefHash rhash;
    cfile ver_window;
    unsigned int type = RH_SORT_HASH;
    unsigned long int seed_len, start=0, end=0, gap_req;
    unsigned char hash_created;
/*    v1printf("making initial 512 run\n");
    v1printf("initing hash, sample_rate(%f == %u)\n", 
	(float)COMPUTE_SAMPLE_RATE((float)hash_size, cfile_len(src_cfh)),
	(unsigned int)COMPUTE_SAMPLE_RATE(hash_size, cfile_len(src_cfh)));
    init_RefHash(&rhash, src_cfh, 512,
	COMPUTE_SAMPLE_RATE(hash_size, cfile_len(src_cfh)), 
	hash_size, type);
    print_RefHash_stats(&rhash);
    v1printf("calling first run...\n");
    OneHalfPassCorrecting(buff, &rhash, ver_cfh);
    v1printf("nuking hash\n");
    free_RefHash(&rhash);*/
    DCB_insert(buff);
    DCBufferReset(buff);

    for(seed_len = 128; seed_len >=16; seed_len /= 2) {
	hash_created = 0;
	gap_req = seed_len * MULTIPASS_GAP_KLUDGE;
	v1printf("\nseed size(%lu)...\n\n", seed_len);
	if(buff->DCB.llm.main_count == 0 && (buff->ver_size >= gap_req)) {
	    copen(&ver_window, ver_cfh->raw_fh, cfile_start_offset(ver_cfh), 
		buff->ver_size, NO_COMPRESSOR, CFILE_RONLY);
	    v1printf("detected empty main, and ver_size <= seed_len\n");
	    v1printf("initing hash, sample_rate(%f == %u)\n", 
		(float)COMPUTE_SAMPLE_RATE((float)hash_size, cfile_len(src_cfh)),
		(unsigned int)COMPUTE_SAMPLE_RATE(hash_size, cfile_len(src_cfh)));
	    init_RefHash(&rhash, src_cfh, seed_len, 
		COMPUTE_SAMPLE_RATE(hash_size, cfile_len(src_cfh)), 
		hash_size, type);
	    print_RefHash_stats(&rhash);
	    DCB_llm_init_buff(buff, 128);
	    hash_created =1;
	    OneHalfPassCorrecting(buff, &rhash, &ver_window);
		cclose(&ver_window);
	    DCB_insert(buff);
	} else if(buff->DCB.llm.main_head->ver_pos >= gap_req) {
	    v1printf("detected suitable hole at main_head\n");
	    v1printf("initing hash, sample_rate(%f == %u)\n", 
		(float)COMPUTE_SAMPLE_RATE((float)hash_size, cfile_len(src_cfh)),
		(unsigned int)COMPUTE_SAMPLE_RATE(hash_size, cfile_len(src_cfh)));
	    init_RefHash(&rhash, src_cfh, seed_len, 
		COMPUTE_SAMPLE_RATE(hash_size, cfile_len(src_cfh)), hash_size,
		type);
	    print_RefHash_stats(&rhash);
	    hash_created=1;
	    copen(&ver_window, ver_cfh->raw_fh, cfile_start_offset(ver_cfh), 
		buff->DCB.llm.main->ver_pos, NO_COMPRESSOR, CFILE_RONLY);
	    DCB_llm_init_buff(buff, 128);
	    OneHalfPassCorrecting(buff, &rhash, &ver_window);
	    v2printf("done, commands=%lu\n", buff->DCB.llm.buff_count);
	    DCB_insert(buff);
	    cclose(&ver_window);
	}
	    while(buff->DCB.llm.main != NULL) {
		start = buff->DCB.llm.main->ver_pos + buff->DCB.llm.main->len;
		end = 0;
		if(buff->DCB.llm.main->next == NULL) {
		    if(buff->ver_size - (buff->DCB.llm.main->ver_pos + 
			buff->DCB.llm.main->len) >= gap_req) {
			end = buff->ver_size;
		    } else {
			start = 0;
		    }
		} else {
		    assert(buff->DCB.llm.main->len + 
			buff->DCB.llm.main->ver_pos < buff->ver_size);
		    if(buff->DCB.llm.main->next->ver_pos - (
			buff->DCB.llm.main->ver_pos + buff->DCB.llm.main->len) 
			>= gap_req) {
			end = buff->DCB.llm.main->next->ver_pos;
		    } else {
			start =0;
		    }
		}
		if(start != 0 ) {
		    assert(start <= end - seed_len);
		    if(hash_created==0) {
			v1printf("initing hash, sample_rate(%f == %u)\n", 
			    (float)COMPUTE_SAMPLE_RATE((float)hash_size, 
			    cfile_len(src_cfh)), (unsigned int)
			    COMPUTE_SAMPLE_RATE(hash_size, cfile_len(src_cfh)));
			init_RefHash(&rhash, src_cfh, seed_len, 
			    COMPUTE_SAMPLE_RATE(hash_size, cfile_len(src_cfh)), 
			    hash_size, type);
			print_RefHash_stats(&rhash);
			hash_created=1;
		    }
		    v1printf("handling gap %lu:%lu, size %lu\n", start, end,
			end-start);
		    copen(&ver_window, ver_cfh->raw_fh, start, end, 
			NO_COMPRESSOR, CFILE_RONLY);
		    DCB_llm_init_buff(buff, 128);
		    OneHalfPassCorrecting(buff, &rhash, &ver_window);
		    v2printf("done, commands=%lu\n", buff->DCB.llm.buff_count);
		    DCB_insert(buff);
		    cclose(&ver_window);
#ifdef DEBUG_DCBUFFER
		    assert(DCB_test_llm_main(buff));
#endif
		}
	    DCBufferIncr(buff);
	    }
	//} //see above comments...
	DCBufferReset(buff);
	if(hash_created) {
	    v1printf("nuking hash\n");
	    free_RefHash(&rhash);
	}
    }
    return 0;
}
