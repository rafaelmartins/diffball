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
#include <string.h>
#include "cfile.h"
#include "dcbuffer.h"
#include "formats.h"
#include "defs.h"
#include "options.h"

unsigned int global_verbosity = 0;
unsigned int patch_compressor = 0;
unsigned int output_to_stdout = 0;
unsigned int global_use_md5 = 0;
char *src_format, *trg_format;

struct poptOption options[] = {
    STD_OPTIONS(output_to_stdout),
    FORMAT_OPTIONS("src-format", 's', src_format),
    FORMAT_OPTIONS("trg-format", 't', trg_format),
    POPT_TABLEEND
};

int 
main(int argc, char **argv)
{
    int in_fh, out_fh;
    struct stat in_stat;
    CommandBuffer dcbuff;
    cfile in_cfh, out_cfh;
    poptContext p_opt;
    signed long optr;
    char *src_file, *trg_file;
    unsigned long int src_format_id, trg_format_id=0;
    signed long recon_val=0, encode_result=0;

    p_opt = poptGetContext("convert_delta", argc, (const char **)argv, 
	options, 0);
    while((optr=poptGetNextOpt(p_opt)) != -1) {
	switch(optr) {
	case OVERSION:
	    print_version("convert_delta");
	case OVERBOSE:
	    global_verbosity++;
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
	(stat(src_file, &in_stat)))
	usage(p_opt, 1, "Must specify an existing patch.", NULL);
    if(output_to_stdout) {
	out_fh = 1;
    } else {
	if((trg_file = (char *)poptGetArg(p_opt))==NULL)
	    usage(p_opt, 1, "Must specify a name for the new patch.", NULL);
        if((out_fh = open(trg_file, O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1){
	    v0printf( "error creating output file '%s'\n", trg_file);
	    exit(1);
	}
    }
    if(NULL!= poptGetArgs(p_opt)) {
	usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS),
	    "unknown option");
    }
    if((in_fh = open(src_file, O_RDONLY, 0))==-1) {
	v0printf( "error opening patch '%s'\n", src_file);
	exit(EXIT_FAILURE);
    }
    copen(&in_cfh, in_fh, 0, in_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    if(src_format == NULL) {
	src_format_id = identify_format(&in_cfh);
	if(src_format_id==0) {
	    v0printf( "Couldn't identify the patch format, aborting\n");
	    exit(EXIT_FAILURE);
	} else if((src_format_id >> 16)==1) {
	    v0printf( "Unsupported format version\n");
	    exit(EXIT_FAILURE);
	}
	src_format_id >>= 16;
    } else {
	src_format_id = check_for_format(src_format, strlen(src_format));
	if(src_format_id==0) {
	    v0printf( "Unknown format '%s'\n", src_format);
	    exit(1);
	}
    }
/*    if(src_format_id==BDELTA_FORMAT) {
	v0printf( "this version does not support conversion of bdelta format.\n");
	v0printf( "the stable 0.40 release will however.  Sorry.\n");
	exit(1);
    }*/
    if(trg_format==NULL) {
	usage(p_opt, 1, "new files format is required\n", NULL);
    } else {
	trg_format_id = check_for_format(trg_format, strlen(trg_format));
	if(trg_format_id==0) {
	    v0printf( "Unknown format '%s'\n", trg_format);
	    exit(1);
	}
    }
    poptFreeContext(p_opt);
    DCBufferInit(&dcbuff, 4096,0,0, DCBUFFER_FULL_TYPE);
    if(SWITCHING_FORMAT == src_format_id) {
        recon_val = switchingReconstructDCBuff(&in_cfh, &dcbuff);
    } else if(GDIFF4_FORMAT == src_format_id) {
        recon_val = gdiff4ReconstructDCBuff(&in_cfh, &dcbuff);
    } else if(GDIFF5_FORMAT == src_format_id) {
        recon_val = gdiff5ReconstructDCBuff(&in_cfh, &dcbuff);       
    } else if(BDIFF_FORMAT == src_format_id) {
        recon_val = bdiffReconstructDCBuff(&in_cfh, &dcbuff);       
    } else if(XDELTA1_FORMAT == src_format_id) {
        recon_val = xdelta1ReconstructDCBuff(&in_cfh, &dcbuff, 1);
    } else if(BDELTA_FORMAT == src_format_id) {
        recon_val = bdeltaReconstructDCBuff(&in_cfh, &dcbuff);
    } else if(BSDIFF_FORMAT == src_format_id) {
	v0printf("Sorry, unwilling to do bsdiff conversion in this version.\n");
	v0printf("Try a newer version.\n");
//	recon_val = bsdiffReconstructDCBuff(&in_cfh, &dcbuff);
//    } else if(UDIFF_FORMAT == src_format_id) {
//      recon_val = udiffReconstructDCBuff(&in_cfh, &src_cfh, NULL, &dcbuff);
    }
    v1printf("reconstruction return=%ld\n", recon_val);
    copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY);
    v1printf("outputing patch...\n");
    v1printf("there were %lu commands\n", dcbuff.DCB.full.buffer_count);
    if(GDIFF4_FORMAT == trg_format_id) {
        encode_result = gdiff4EncodeDCBuffer(&dcbuff, &out_cfh);
    } else if(GDIFF5_FORMAT == trg_format_id) {
        encode_result = gdiff5EncodeDCBuffer(&dcbuff, &out_cfh);
    } else if(BDIFF_FORMAT == trg_format_id) {
        encode_result = bdiffEncodeDCBuffer(&dcbuff, &out_cfh);
    } else if(SWITCHING_FORMAT == trg_format_id) {
        encode_result = switchingEncodeDCBuffer(&dcbuff, &out_cfh);
    } else if (BDELTA_FORMAT == trg_format_id) {
        encode_result = bdeltaEncodeDCBuffer(&dcbuff, &out_cfh);
    }
    v1printf("encoding return=%ld\n", encode_result);
    v1printf("finished.\n");
    DCBufferFree(&dcbuff);
    cclose(&in_cfh);
    cclose(&out_cfh);
    return 0;
}
