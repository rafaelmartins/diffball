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
    struct stat src_stat;
    int src_fh, delta_fh, out_fh;
    char *src, *trg;
    if(argc <4){
	printf("pardon, but...\nI need at least 3 args- (reference file), patch-file, target file\n");
	exit(EXIT_FAILURE);
    }
    if(stat(argv[1], &src_stat)) {
	perror("what the hell, stat failed.  wtf?\n");
	exit(1);
    }
    printf("src_fh size=%lu\n", src_stat.st_size);
    if ((src_fh = open(argv[1], O_RDONLY,0)) == -1) {
	printf("Couldn't open %s, does it exist?\n", argv[1]);
	exit(EXIT_FAILURE);
    }
    if((delta_fh = open(argv[2], O_RDONLY,0))==-1) {
	printf("Couldn't open patch file.\n");
	exit(EXIT_FAILURE);
    }
    if((out_fh = open(argv[2], O_RDWR | O_TRUNC | O_CREAT,0))==-1) {
	printf("Couldn't create\truncate output file.\n");
	exit(EXIT_FAILURE);
    }
    
}
