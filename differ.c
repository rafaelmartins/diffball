#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "adler32.h"

int main(int argc, char **argv)
{
    unsigned long int x;
    int src_fh, trg_fh, out_fh;
    if(argc <3){
	printf("pardon, but...\nI need at least 2 args- (source file), (target file), [patch-file]\n");
	exit(EXIT_FAILURE);
    }
    if ((src_fh = open(argv[1], O_RDONLY,0)) == -1) {
	printf("Couldn't open %s, does it exist?\n", argv[1]);
	exit(EXIT_FAILURE);
    }
    if ((trg_fh = open(argv[2], O_RDONLY,0)) == -1) {
	printf("Couldn't open %s, does it exist?\n", argv[2]);
	exit(EXIT_FAILURE);
    }
    if(argc==3) {
	if((out_fh = dup(0))==-1){
	    printf("well crud, couldn't duplicate stdout.  Likely a bug in differ.\n");
	    exit(EXIT_FAILURE);
	}
    } else if((out_fh = open(argv[3], O_RDWR | O_TRUNC | O_CREAT,0))==-1) {
	printf("Couldn't create\truncate patch file.\n");
	exit(EXIT_FAILURE);
    } else {
	fprintf(stderr,"storing generated delta in '%s'\n", argv[3]);
    }
    
}
