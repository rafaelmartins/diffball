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
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cfile.h"
#include "diff-algs.h"
//#include "bit-functions.h"
#include "string-misc.h"
#include "gdiff.h"
#include "bdiff.h"
#include "bdelta.h"

unsigned long convertDec(unsigned char *buff, unsigned int len)
{
    unsigned long num=0,x=0;
    while(len > x)
	num = (num * 10) + (buff[x++] - 48);
    return num;
}

int main(int argc, char **argv)
{
    struct stat ref_stat, ver_stat;
    cfile out_cfh, ref_cfh, ver_cfh;
    int ref_fh, ver_fh, out_fh;
    //char *src, *trg;
    CommandBuffer buffer;
    RefHash rhash;
    unsigned long seed_len, multi;
    unsigned int offset_type;
    if(argc <3){
		printf("pardon, but...\nI need at least 2 args- (source file), (target file), [patch-file]\n");
		exit(EXIT_FAILURE);
    }
    if(stat(argv[1], &ref_stat)) {
		perror("what the hell, stat failed.  wtf?\n");
		exit(1);
    }
    if(stat(argv[2], &ver_stat)) {
		perror("what the hell, stat failed.  wtf?\n");
		exit(1);
    }
    //printf("src_fh size=%lu\ntrg_fh size=%lu\n", 
    //	(unsigned long)src_stat.st_size, (unsigned long)ver_stat.st_size);
    if ((ref_fh = open(argv[1], O_RDONLY,0)) == -1) {
		printf("Couldn't open %s, does it exist?\n", argv[1]);
		exit(EXIT_FAILURE);
    }
    if ((ver_fh = open(argv[2], O_RDONLY,0)) == -1) {
		printf("Couldn't open %s, does it exist?\n", argv[2]);
		exit(EXIT_FAILURE);
    }
/*    if(argc==3) {
		if((out_fh = dup(0))==-1){
	    	printf("well crud, couldn't duplicate stdout.  Likely a bug in differ.\n");
	   		exit(EXIT_FAILURE);
		}
    } else */
    if((out_fh = open(argv[3], O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
		printf("Couldn't create\truncate patch file.\n");
		exit(EXIT_FAILURE);
    } else {
		fprintf(stderr,"storing generated delta in '%s'\n", argv[3]);
    }
    copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY);
    if(argc < 5) {
		seed_len = 16;
		multi = ref_stat.st_size;
    } else {	     
		seed_len = convertDec(argv[4], strlen(argv[4]));
	if(argc < 6) {
		    multi = ref_stat.st_size;
		} else {
	    	multi = convertDec(argv[5], strlen(argv[5]));
		}
    }
    printf("using seed_len(%lu), multi(%lu)\n", seed_len, multi);
    DCBufferInit(&buffer, 1000000, ref_stat.st_size, ver_stat.st_size);
    copen(&ref_cfh, ref_fh, 0, ref_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    init_RefHash(&rhash, &ref_cfh, seed_len, 6, ref_cfh.byte_len/6);
    copen(&ver_cfh, ver_fh, 0, ver_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    printf("opened\n");
    OneHalfPassCorrecting(&buffer, &rhash, &ver_cfh);
    cclose(&ref_cfh);
    printf("outputing patch...\n");
    offset_type = ENCODING_OFFSET_START;
//    offset_type = ENCODING_OFFSET_DC_POS;
    bdiffEncodeDCBuffer(&buffer, &ver_cfh, &out_cfh);
//    gdiffEncodeDCBuffer(&buffer, offset_type, &ver_cfh, &out_cfh);
//    switchingEncodeDCBuffer(&buffer, offset_type, &ver_cfh, &out_cfh);
//    rawEncodeDCBuffer(&buffer, offset_type, &ver_cfh, &out_cfh);
    bdeltaEncodeDCBuffer(&buffer, &ver_cfh, &out_cfh);
    printf("exiting\n");
    cclose(&ver_cfh);
    cclose(&out_cfh);
    return 0;
}
