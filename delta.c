#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "adler32.h"
#include "delta.h"
/* this is largely based on the algorithms detailed in randal burn's various papers.
   Obviously credit for the alg's go to him, although I'm the one who gets the dubious
   credit for bugs in the implementation of said algorithms... */
#define LOOKBACK_SIZE 100000

inline void dc_add(struct deltaCommandLoc


inline unsigned long hash_it(unsigned long chk, unsigned long tbl_size)
{
    return chk % tbl_size;
}

char *OneHalfPassCorrecting(unsigned char *ref, unsigned long ref_len,
    unsigned char *ver, unsigned long ver_len, unsigned int seed_len)
{
    unsigned long *hr; //reference hash table.
    unsigned long x, index, len;
    unsigned long s1, s2;
    unsigned long empties=0, good_collisions=0, bad_collisions=0;
    unsigned char *vc, *va, *vs; //va=adler start, vs=first non-encoded byte.
    struct deltaCommandLoc *dc_buffer, *dc_head, *dc_tail, *dc_buffer_end;
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
    printf("load factor=%f\%\n", ((float)empties)/(float)(ref_len-seed_len-good_collisions)*(float)100);
    printf("bad collisions=%f\%\n", ((float)bad_collisions) / (float)(ref_len-seed_len-good_collisions)*(float)100);
    printf("good collisions=%f\%\n", ((float)good_collisions)/(float)(ref_len-seed_len)*100);
    /*for(x=0; x < ref_len - seed_len; x++){
	if (hr[x])
	    printf("hr[%u]='%lu'\n", x, hr[x]);
    }*/
    printf("version run:\n");
    printf("creating lookback buffer\n");
    if((dc_buffer=(struct deltaCommandLoc *)malloc(sizeof(struct deltaCommandLoc)*LOOKBACK_SIZE))==NULL){
	perror("Shite.  couldn't allocate needed memory for lookback block.\n");
	exit(EXIT_FAILURE);
    }
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
		printf("bad collision(%lu).\n", (unsigned char *)vc - (unsigned char*)ver);
		bad_collisions++;
		vc++;
		continue;
	    }
	    printf("good collision(%lu):", (unsigned char *)vc - (unsigned char*)ver);
	    good_collisions++;
	    x=0;
	    len=16;
	    while(vc -x > ver && hr[index] -x > 0) {
		if(vc[-x]==ref[hr[index]-x]) {
		    x++;
		} else {
		    break;
		}
	    }
	    while(vc + len < ver + ver_len && hr[index] + x < ref_len) {
		if(vc[len]==ref[hr[index]+len]) {
		    len++;
		} else {
		    break;
		}
	    }
	    printf("vstart(%lu), rstart(%lu), len(%lu)\n", (unsigned char*)vc -x - (unsigned char*)ver,
		hr[index] - x, len);
	    if (vs <= vm) {
		if (vs!
	    } else if (vm < vs) {
		
	    } else {
		printf("what in the fuck... hit 3rd conditional on correction.  this means what?\n");
		exit(EXIT_FAILURE);
	    }
	    vc += len;
	    continue;
	}
	printf("no match(%lu)\n", vc -ver);
	vc++;
	continue;
    }
    printf("version summary:\n");
    printf("good collisions(%f\%)\n", (float)good_collisions/(float)(good_collisions+bad_collisions)*100);
    printf("bad  collisions(%f\%)\n", (float)bad_collisions/(float)(good_collisions+bad_collisions)*100);
    return NULL;
}

//unsigned long hash_it(unsigned long

