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
#include "formats.h"
#include "cfile.h"
#include "defs.h"
#include "options.h"

unsigned int verbosity=0;
unsigned long sample_rate=0;
unsigned long seed_len = 0;
unsigned long hash_size = 0;
unsigned int patch_compressor = 0;
unsigned int patch_to_stdout = 0;
unsigned int use_md5 = 0;
char  *patch_format;

struct poptOption options[] = {
    STD_OPTIONS(patch_to_stdout),
    DIFF_OPTIONS(seed_len, sample_rate,hash_size),
    FORMAT_OPTIONS("patch-format", 'f', patch_format),
    MD5_OPTION(use_md5),
    POPT_TABLEEND
};

int main(int argc, char **argv)
{
    struct stat ref_stat, ver_stat;
    cfile out_cfh, ref_cfh, ver_cfh;
    int ref_fh, ver_fh, out_fh;
    //char *src, *trg;
    CommandBuffer buffer;
    RefHash rhash;
    unsigned int offset_type;
    poptContext p_opt;

    signed long optr;
    char  *src_file;
    char  *trg_file;
    char  *patch_name;
    unsigned long patch_id = 0;
    signed long encode_result=0;

    p_opt = poptGetContext("differ", argc, (const char **)argv, options, 0);
    while((optr=poptGetNextOpt(p_opt)) != -1) {
	if(optr < -1) {
	    usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS), 
		poptStrerror(optr));
	}
	switch(optr) {
	case OVERSION:
	    // print version.
	    exit(0);
	case OVERBOSE:
	    verbosity++;
	    break;
	case OBZIP2:
	    if(patch_compressor) {
		// bitch at em.
	    } else
		patch_compressor = BZIP2_COMPRESSOR;
	    break;
	case OGZIP:
	    if(patch_compressor) {
		// bitch at em.
	    } else 
		patch_compressor = GZIP_COMPRESSOR;
	    break;
	}
    }
    if( ((src_file=(char *)poptGetArg(p_opt))==NULL) || 
	(stat(src_file, &ref_stat))) 
	usage(p_opt, 1, "Must specify an existing source file.",NULL);
    if( ((trg_file=(char *)poptGetArg(p_opt))==NULL) || 
	(stat(trg_file, &ver_stat)) )
	usage(p_opt, 1, "Must specify an existing target file.",NULL);
    if(patch_to_stdout != 0) {
	out_fh = 0;
    } else {
	if((patch_name = poptGetArg(p_opt))==NULL)
	    usage(p_opt, 1, "Must specify a name for the patch file.",NULL);
	if((out_fh = open(patch_name, O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
	    fprintf(stderr, "error creating patch file '%s' (open failed)\n",
	    patch_name);
	    exit(1);
	}
    }
    if (NULL!=poptGetArgs(p_opt)) {
	usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS),
	"unknown option");
    }
    poptFreeContext(p_opt);
    if ((ref_fh = open(src_file, O_RDONLY,0)) == -1) {
	fprintf(stderr, "error opening src_file\n");
	exit(EXIT_FAILURE);
    }
    if ((ver_fh = open(trg_file, O_RDONLY,0)) == -1) {
	fprintf(stderr, "error opening trg_file\n");
	exit(EXIT_FAILURE);
    }
    copen(&ref_cfh, ref_fh, 0, ref_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    copen(&ver_cfh, ver_fh, 0, ver_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    copen(&out_cfh, out_fh, 0, 0, patch_compressor, CFILE_WONLY);
    if(sample_rate==0) {
	/* implement a better assessment based on mem and such */
	sample_rate = 1;
    }
    if(hash_size==0) {
	/* implement a better assessment based on mem and such */
	hash_size = ref_stat.st_size;
    }
    if(seed_len==0) {
	seed_len = DEFAULT_SEED_LEN;
    }
    if(patch_format==NULL) {
	patch_id = DEFAULT_PATCH_ID;
    } else {
	patch_id = check_for_format(patch_format, strlen(patch_format));
	if(patch_id==0) {
	    fprintf(stderr, "Unknown format '%s'\n", patch_format);
	    exit(1);
	}
    }
    v1printf("using patch format %lu\n", patch_id);
    v1printf("using seed_len(%lu), sample_rate(%lu), hash_size(%lu)\n", 
	seed_len, sample_rate, hash_size);
    v1printf("verbosity level(%u)\n", verbosity);
    DCBufferInit(&buffer, 1000000, ref_stat.st_size, ver_stat.st_size);
    init_RefHash(&rhash, &ref_cfh, seed_len, sample_rate, hash_size);
    OneHalfPassCorrecting(&buffer, &rhash, &ver_cfh);
    v1printf("outputing patch...\n");
    v1printf("there were %lu commands\n", buffer.buffer_count);
    if(GDIFF4_FORMAT == patch_id) {
	encode_result = gdiff4EncodeDCBuffer(&buffer, &ver_cfh, &out_cfh);
    } else if(GDIFF5_FORMAT == patch_id) {
	encode_result = gdiff5EncodeDCBuffer(&buffer, &ver_cfh, &out_cfh);
    } else if(BDIFF_FORMAT == patch_id) {
	encode_result = bdiffEncodeDCBuffer(&buffer, &ver_cfh, &out_cfh);
    } else if(SWITCHING_FORMAT == patch_id) {
	encode_result = switchingEncodeDCBuffer(&buffer, &ver_cfh, &out_cfh);
    } else if (BDELTA_FORMAT == patch_id) {
	encode_result = bdeltaEncodeDCBuffer(&buffer, &ver_cfh, &out_cfh);
    }
//    offset_type = ENCODING_OFFSET_START;
//    offset_type = ENCODING_OFFSET_DC_POS;
//    bdiffEncodeDCBuffer(&buffer, &ver_cfh, &out_cfh);
//    gdiffEncodeDCBuffer(&buffer, offset_type, &ver_cfh, &out_cfh);
//    switchingEncodeDCBuffer(&buffer, offset_type, &ver_cfh, &out_cfh);
//    rawEncodeDCBuffer(&buffer, offset_type, &ver_cfh, &out_cfh);
//    bdeltaEncodeDCBuffer(&buffer, &ver_cfh, &out_cfh);
    v1printf("encode_result=%ld\n", encode_result);
    v1printf("exiting\n");
    free_RefHash(&rhash);
    DCBufferFree(&buffer);
    cclose(&ref_cfh);
    cclose(&ver_cfh);
    cclose(&out_cfh);
    return 0;
}
