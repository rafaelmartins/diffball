#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "cfile.h"
#include "dcbuffer.h"
#include "gdiff.h"
#include "switching.h"
#include "raw.h"

int main(int argc, char **argv) {
    int din_fh, dout_fh;
    struct stat din_stat;
    unsigned int offset_type;
    struct CommandBuffer dcbuff;
    struct cfile din_cfh, dout_cfh;
    if(argc < 2) {
	printf("pardon, but I need at least 2 args- source file, new file...\n");
	exit(EXIT_FAILURE);
    }
    if(stat(argv[1], &din_stat)) {
	printf("what the hell, stat failed.  wtf?\n");
	exit(EXIT_FAILURE);
    } else if ((din_fh = open(argv[1], O_RDONLY,0))==-1) {
	printf("couldn't open %s, eh?\n", argv[1]);
	exit(EXIT_FAILURE);
    }
    if((dout_fh = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
	printf("Couldn't create/trauncate new patch file.\n");
	exit(EXIT_FAILURE);
    }
    copen(&din_cfh, din_fh, 0, din_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    copen(&dout_cfh, dout_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY);
    DCBufferInit(&dcbuff, 1000000);
    offset_type = ENCODING_OFFSET_DC_POS;
    printf("reconstructing dcbuffer...\n");
    gdiffReconstructDCBuff(&din_cfh, &dcbuff, offset_type, 4);
    printf("outputing patch...\n");
    switchingEncodeDCBuffer(&dcbuff, offset_type, &din_cfh, &dout_cfh);
    printf("finished.\n");
    cclose(&din_cfh);
    cclose(&dout_cfh);
    return 0;
}
