#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "adler32.h"
/* this is largely based on the algorithms detailed in randal burn's various papers.
   Obviously credit for the alg's go to him, although I'm the one who gets the dubious
   credit for bugs in the implementation of said algorithms... */

char *OneHalfPassCorrecting(char *ref, unsigned long ref_len, unsigned int seed_len)/* ,
    const char *ver, unsigned long ver_len, unsigned int seed_len)*/
{
    unsigned long *hr; //reference hash table.
    unsigned long x;
    unsigned long s1, s2;
    s1=s2=0;
    if((hr=(unsigned long*)malloc(sizeof(unsigned long)*(ref_len - seed_len)))==NULL) {
	perror("Shite.  couldn't allocate needed memory for reference hash table.\n");
	exit(EXIT_FAILURE);
    }
    // init the bugger==0
    for(x=0; x < ref_len - seed_len; x++)
	hr[x] = 0;
    /* generate footprints.  note doing the summing here, since it's a rolling checksum. */
    for(x=0; x < seed_len; x++) {
        s1 += ref[x]; s2 += s1;
    }
    hr[0] = ((s2 <<16) | (s1 & 0xffff));

    for(x=seed_len; x < ref_len - seed_len-1; x++) {
	s1 = s1 - ref[x-seed_len] + ref[x];
	s2 = s2 - (seed_len * ref[x-seed_len]) + s1;
	hr[x - seed_len+1];
    }
    printf("chksum array genned\n");
    /*for(x=0; x < ref_len - seed_len; x++){
	printf("hr[%u]='%lu'\n", x, hr[x]);
    }*/
    return NULL;
}

//unsigned long hash_it(unsigned long

