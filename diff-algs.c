#include <stdlib.h>
//#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
//#include "adler32.h"
#include "diff-algs.h"
#include "bit-functions.h"
#include "gdiff.h"

/* this is largely based on the algorithms detailed in randal burn's various papers.
   Obviously credit for the alg's go to him, although I'm the one who gets the dubious
   credit for bugs in the implementation of said algorithms... */
//#define LOOKBACK_SIZE 100000
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

inline unsigned long hash_it(unsigned long chk, unsigned long tbl_size)
{
    return chk % tbl_size;
}

char *OneHalfPassCorrecting(unsigned int encoding_type,
    unsigned int offset_type, 
    unsigned char *ref, unsigned long ref_len,
    unsigned char *ver, unsigned long ver_len, unsigned int seed_len, /*int out_fh*/ 
    struct cfile *out_fh)
{
    unsigned long *hr; //reference hash table.
    unsigned long x, index, len;
    unsigned long s1, s2;
    unsigned long empties=0, good_collisions=0, bad_collisions=0;
    unsigned char *vc, *va, *vs, *vm, *rm; //va=adler start, vs=first non-encoded byte.
    struct CommandBuffer buffer;
    unsigned long copies=0, adds=0, truncations=0;
    s1=s2=0;
    if((hr=(unsigned long*)malloc(sizeof(unsigned long)*(ref_len - seed_len)))==NULL) {
	perror("Shite.  couldn't allocate needed memory for reference hash table.\n");
	exit(EXIT_FAILURE);
    }
    // init the bugger==0
    for(x=0; x < ref_len - seed_len; x++)
	hr[x] = 0;
    empties++;
    for(x=0; x < seed_len; x++) {
        s1 += ref[x]; s2 += s1;
    }
    hr[hash_it((s2 <<16) | (s1 & 0xffff), ref_len-seed_len)] =0;

    for(x=seed_len; x < ref_len - seed_len-1; x++) {
	s1 = s1 - ref[x-seed_len] + ref[x];
	s2 = s2 - (seed_len * ref[x-seed_len]) + s1;
	//hr[x - seed_len+1];
	index=hash_it((s2<<16)|(s1 & 0xffff), ref_len-seed_len);
	/*note this has the ability to overwrite offset 0...
	  but thats alright, cause a correcting alg if a match at offset1, will grab the offset 0 */
	if(hr[index]==0) {
	    empties++;
	    hr[index] = x - seed_len+1;
	} else {
	    if(memcmp((unsigned char *)ref+hr[index], (unsigned char*)ref+x, seed_len)==0){
		good_collisions++;
	    } else {
		bad_collisions++;
	    }
	}
    }
    printf("reference run:\n");
    printf("chksum array(%lu) genned\n", ref_len-seed_len);
    printf("load factor=%f%%\n", ((float)empties)/(float)(ref_len-seed_len-good_collisions)*(float)100);
    printf("bad collisions=%f%%\n", ((float)bad_collisions) / (float)(ref_len-seed_len-good_collisions)*(float)100);
    printf("good collisions=%f%%\n", ((float)good_collisions)/(float)(ref_len-seed_len)*100);
    /*for(x=0; x < ref_len - seed_len; x++){
	if (hr[x])
	    printf("hr[%u]='%lu'\n", x, hr[x]);
    }*/
    printf("version run:\n");
    printf("creating lookback buffer\n");
    DCBufferInit(&buffer, 10000000);
    good_collisions = bad_collisions =0;
    vs = vc = (unsigned char*)ver;
    va=NULL; //this is the starting pt of the adler chksum of len(seed_len).
    while(vc + seed_len < (unsigned char *)ver + ver_len) {
	if(vc - seed_len < va) {
	    for(; va < vc; va++) {
		s1 = s1 - *va + va[seed_len];
		s2 = s2 - (seed_len * (unsigned char)*va) + s1;
	    }
	} else {
	    s1=s2=0;
	    for(va=vc; va < vc + seed_len; va++){
		s1 += *va;
		s2 += s1;
	    }
	    va=vc;
	}
	index = hash_it((s2 << 16) | (s1 & 0xffff), ref_len -seed_len);
	if(hr[index]) {
	    if (memcmp(ref+hr[index], vc, seed_len)!=0){
		//printf("bad collision(%lu).\n", (unsigned char *)vc - (unsigned char*)ver);
		bad_collisions++;
		vc++;
		continue;
	    }
	    printf("good collision(%lu):", (unsigned long)((unsigned char *)vc - (unsigned char*)ver));
	    good_collisions++;
	    x=0;
	    vm = vc;
	    rm = ref + hr[index];
	    while(vm > ver && rm > ref &&
		*(vm -1)==*(rm -1)) {
		vm--;
		rm--;
	    }
	    len = (vc - vm) + seed_len;
	    //printf("prefix len(%lu), ",len-seed_len);
	    while(vm + len < ver + ver_len && rm + len < ref + ref_len && vm[len] == rm[len]) {
		len++;
	    }
	    //printf("couldn't match %u==%u\n", vm[len], rm[len]);
	    printf("vstart(%lu), rstart(%lu), len(%lu)\n", (unsigned long)((unsigned char*)vm - (unsigned char*)ver),
		(unsigned long)(rm -ref), len);
	    if (vs <= vm) {
		if (vs < vm) {
		    printf("    adding vstart(%lu), len(%lu), vend(%lu): (vs < vm)\n",
			(unsigned long)(vs -ver), (unsigned long)(vm-vs), (unsigned long)(vm - ver));
		    //DCBufferAddCmd(&buffer, DC_ADD, vs -ver, (vc-x) -vs);
		    DCBufferAddCmd(&buffer, DC_ADD, vs -ver, vm - vs);
		    adds++;
		}
		printf("    copying offset(%lu), len(%lu)\n", (unsigned long)(vm -ver), len);
		//DCBufferAddCmd(&buffer, DC_COPY, (vc-x) - ver, len);
		//DCBufferAddCmd(&buffer, DC_COPY, hr[index] -x, len +x);
		DCBufferAddCmd(&buffer, DC_COPY, rm - ref, len);
	    } else if (vm < vs) {
		printf("    truncating(%lu) bytes: (vm < vs)\n", (unsigned long)(vs - vm));
		DCBufferTruncate(&buffer, vs - vm);
		printf("    replacement copy: offset(%lu), len(%lu)\n", (unsigned long)(rm - ref), len);
		DCBufferAddCmd(&buffer, DC_COPY, rm -ref, len);
		truncations++;
	    } else {
		printf("what in the fuck... hit 3rd conditional on correction.  this means what?\n");
		exit(EXIT_FAILURE);
	    }
	    copies++;
	    vs = vm + len ;
	    vc = vs -1;
	} else {
	    //printf("no match(%lu)\n", vc -ver);
	}
	vc++;
    }
    if (vs -ver != ver_len)
	DCBufferAddCmd(&buffer, DC_ADD, vs -ver, ver_len - (vs -ver));
    printf("version summary:\n");
    printf("good collisions(%f%%)\n", (float)good_collisions/(float)(good_collisions+bad_collisions)*100);
    printf("bad  collisions(%f%%)\n",(float)bad_collisions/(float)(good_collisions+bad_collisions)*100);
    printf("commands in buffer, copies(%lu), adds(%lu), truncations(%lu)\n", copies, adds, truncations);
    printf("\n\nflushing command buffer...\n\n\n");
    //DCBufferFlush(&buffer, ver, out_fh);
    if(encoding_type==USE_GDIFF_ENCODING) {
	printf("using gdiff encoding...\n");
	if(gdiffEncodeDCBuffer(&buffer, offset_type, ver, out_fh)) {
	    printf("wtf? error returned from encoding engine\n");
	}
    }
    return NULL;
}
