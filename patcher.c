#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "delta.h"
#include "pdbuff.h"
#include "gdiff.h"
//#include "delta.h"


//offset = fh_pos + readSignedBytes(cpy_buff, ctmp);
//len = readUnsignedBytes(cpy_buff+ctmp, clen);


int main(int argc, char **argv)
{
    unsigned long int x;
    struct stat src_stat, delta_stat;
    int src_fh, delta_fh, out_fh;
    struct PatchDeltaBuffer PDBuff;
    if(argc <4){
	printf("pardon, but...\nI need at least 3 args- (reference file), patch-file, target file\n");
	exit(EXIT_FAILURE);
    }
    if(stat(argv[1], &src_stat)) {
	perror("what the hell, stat failed.  wtf?\n");
	exit(EXIT_FAILURE);
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
    if(stat(argv[2], &delta_stat)) {
	perror("what the hell, stat failed.  wtf?\n");
	exit(EXIT_FAILURE);
    }
    printf("delta_fh size=%lu\n", delta_stat.st_size);
    if((out_fh = open(argv[3], O_RDWR | O_TRUNC | O_CREAT,0))==-1) {
	printf("Couldn't create\truncate output file.\n");
	exit(EXIT_FAILURE);
    }
    /*signed int gdiffReconstructFiles(int src_fh, int out_fh,
    struct PatchDeltaBuffer *PDBuff, unsigned int offset_type,
    unsigned int gdiff_version);*/
    initPDBuffer(&PDBuff, delta_fh, 5, 4096);
    printf("here goes...\n");
    printf("dumping initial buffer\n");
    printf("initial value(%u)\n", PDBuff.buffer[0]);
    printf("filled_len(%u), buff(%u)\n", PDBuff.filled_len, PDBuff.buff_size);
    //printf("%*s\n", 5800, PDBuff.buffer);
    gdiffReconstructFile(src_fh, out_fh, &PDBuff, ENCODING_OFFSET_START, 4);
}

