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
#include "bdiff.h"

int 
main(int argc, char **argv)
{
    int din_fh, dout_fh, dout2_fh;
    struct stat din_stat;
    unsigned int offset_type;
    CommandBuffer dcbuff;
    cfile din_cfh, dout_cfh, dout2_cfh;
    if(argc < 3) {
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
    if(argc > 3) {
	if((dout2_fh = open(argv[3], O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
		printf("Couldn't create/trauncate new patch file.\n");
		exit(EXIT_FAILURE);
	}
    }
    copen(&din_cfh, din_fh, 0, din_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    copen(&dout_cfh, dout_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY);
    DCBufferInit(&dcbuff, 1000000,0,0);
//    offset_type = ENCODING_OFFSET_START;
    offset_type = ENCODING_OFFSET_DC_POS;
    printf("reconstructing dcbuffer...\n");
//    switchingReconstructDCBuff(&din_cfh, &dcbuff, offset_type);
//    gdiffReconstructDCBuff(&din_cfh, &dcbuff, offset_type, 4);
    bdiffReconstructDCBuff(&din_cfh, &dcbuff);
    DCBufferCollapseAdds(&dcbuff);
    printf("outputing patch...\n");
    gdiffEncodeDCBuffer(&dcbuff, offset_type, &din_cfh, &dout_cfh);
//    switchingEncodeDCBuffer(&dcbuff, offset_type, &din_cfh, &dout_cfh);
/*    if(argc > 3) {
	copen(&dout2_cfh, dout2_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY);
	printf("outputting second patch.\n");
	cclose(&dout2_cfh);
    }*/
    printf("finished.\n");
    DCBufferFree(&dcbuff);
    cclose(&din_cfh);
    cclose(&dout_cfh);
    return 0;
}
