#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "delta.h"

int main(int argc, char **argv)
{
    unsigned long int x;
    struct stat src_stat, trg_stat;
    int src_fh, trg_fh, out_fh;
    char *src, *trg;
    if(argc <3){
	printf("pardon, but...\nI need at least 2 args- (source file), (target file), [patch-file]\n");
	exit(EXIT_FAILURE);
    }
    if(stat(argv[1], &src_stat)) {
	perror("what the hell, stat failed.  wtf?\n");
	exit(1);
    }
    if(stat(argv[2], &trg_stat)) {
	perror("what the hell, stat failed.  wtf?\n");
	exit(1);
    }
    printf("src_fh size=%lu\ntrg_fh size=%lu\n", src_stat.st_size, trg_stat.st_size);
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
    } else if((out_fh = open(argv[3], O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
		printf("Couldn't create\truncate patch file.\n");
		exit(EXIT_FAILURE);
    } else {
		fprintf(stderr,"storing generated delta in '%s'\n", argv[3]);
    }
    src=(char*)malloc(src_stat.st_size);
    trg=(char*)malloc(trg_stat.st_size);
    read(src_fh, src, src_stat.st_size);
    read(trg_fh, trg, trg_stat.st_size);
    /*OneHalfPassCorrecting(USE_GDIFF_ENCODING, ENCODING_OFFSET_START, src, (unsigned long)src_stat.st_size,
	trg, trg_stat.st_size, 16, out_fh);*/
    OneHalfPassCorrecting(USE_GDIFF_ENCODING, ENCODING_OFFSET_START, src, 
    	(unsigned long)src_stat.st_size,
	trg, trg_stat.st_size, 16, out_fh);
    return 0;
}
