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
//#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "string-misc.h"
#include "cfile.h"
#include "dcbuffer.h"
#include "apply-patch.h"
#include "formats.h"
#include "defs.h"
#include "options.h"

unsigned int global_verbosity = 0;
unsigned int out_compressor = 0;
unsigned int output_to_stdout = 0;
unsigned int global_use_md5 = 0;
char  *patch_format;

struct poptOption options[] = {
    STD_OPTIONS(output_to_stdout),
    FORMAT_OPTIONS("patch-format", 'f', patch_format),
    POPT_TABLEEND
};

int
main(int argc, char **argv)
{
    struct stat src_stat, patch_stat;
    int src_fh, patch_fh, out_fh;
    cfile src_cfh, patch_cfh, out_cfh;
    CommandBuffer dcbuff;
    poptContext p_opt;

    signed long optr;
    char  *src_name;
    char  *out_name;
    char  *patch_name;
    unsigned long int patch_id=0;
    signed long int recon_val=0;

    p_opt = poptGetContext("patcher", argc, (const char **)argv, options, 0);
    while((optr=poptGetNextOpt(p_opt)) != -1) {
	if(optr < -1) {
	    usage(p_opt, 1, poptBadOption(p_opt,
		POPT_BADOPTION_NOALIAS), poptStrerror(optr));
	}
	switch(optr) {
	case OVERSION:
	    print_version("patcher");
	case OVERBOSE:
	    global_verbosity++;
	    break;
	case OBZIP2:
	    if(out_compressor) {
		// bitch at em.
	    } else
		out_compressor = BZIP2_COMPRESSOR;
	    break;
	case OGZIP:
	    if(out_compressor) {
		// bitch at em.
	    } else 
		out_compressor = GZIP_COMPRESSOR;
	    break;
	}
    }
    if( ((src_name=(char *)poptGetArg(p_opt))==NULL) || 
	(stat(src_name, &src_stat))) 
	usage(p_opt, 1, "Must specify an existing source file.", 0);
    if( ((patch_name=(char *)poptGetArg(p_opt))==NULL) || 
	(stat(patch_name, &patch_stat)) )
	usage(p_opt, 1, "Must specify an existing patch file.", 0);
    if(output_to_stdout != 0) {
	out_fh = 0;
    } else {
	if((out_name = (char *)poptGetArg(p_opt))==NULL)
	    usage(p_opt, 1, "Must specify a name for the out file.", 0);
	if((out_fh = open(out_name, O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
	    fprintf(stderr, "error creating out file (open failed)\n");
	    exit(1);
	}
    }
    if (NULL!=poptGetArgs(p_opt)) {
	usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS), 
	"unknown option");
    }	
    poptFreeContext(p_opt);
    v1printf("verbosity level(%u)\n", global_verbosity);
    if ((src_fh = open(src_name, O_RDONLY,0)) == -1) {
	fprintf(stderr,"error opening source file '%s'\n", src_name);
	exit(EXIT_FAILURE);
    }
    if((patch_fh = open(patch_name, O_RDONLY,0))==-1) {
	fprintf(stderr, "error opening patch file '%s'\n", patch_name);
	exit(EXIT_FAILURE);
    }
    v1printf("src_fh size=%lu\n", (unsigned long)src_stat.st_size);
    v1printf("patch_fh size=%lu\n", (unsigned long)patch_stat.st_size);
    copen(&src_cfh, src_fh, 0, src_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    copen(&patch_cfh, patch_fh, 0, patch_stat.st_size, 
	NO_COMPRESSOR, CFILE_RONLY);
    if(patch_format==NULL) {
	patch_id = identify_format(&patch_cfh);
	if(patch_id==0) {
	    fprintf(stderr, "Couldn't identify the patch format, aborting\n");
	    exit(EXIT_FAILURE);
	} else if((patch_id & 0xffff)==1) {
	    fprintf(stderr, "Unsupported format version\n");
	    exit(EXIT_FAILURE);
	}
	patch_id >>=16;
    } else {
	patch_id = check_for_format(patch_format, strlen(patch_format));
	if(patch_id==0) {
	    fprintf(stderr, "Unknown format '%s'\n", patch_format);
	    exit(1);
	}
    }
    v1printf("patch_type=%lu\n", patch_id);
    cseek(&patch_cfh, 0, CSEEK_FSTART);
    copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY);
	DCBufferInit(&dcbuff, 4096, 0, 0);
    if(SWITCHING_FORMAT == patch_id) {
	recon_val = switchingReconstructDCBuff(&patch_cfh, &dcbuff);
    } else if(GDIFF4_FORMAT == patch_id) {
	recon_val = gdiff4ReconstructDCBuff(&patch_cfh, &dcbuff);
    } else if(GDIFF5_FORMAT == patch_id) {
	recon_val = gdiff5ReconstructDCBuff(&patch_cfh, &dcbuff);
    } else if(BDIFF_FORMAT == patch_id) {
	recon_val = bdiffReconstructDCBuff(&patch_cfh, &dcbuff);
    } else if(XDELTA1_FORMAT == patch_id) {
	recon_val = xdelta1ReconstructDCBuff(&patch_cfh, &dcbuff, 1);
    } else if(BDELTA_FORMAT == patch_id) {
	recon_val = bdeltaReconstructDCBuff(&patch_cfh, &dcbuff);
//    } else if(UDIFF_FORMAT == patch_id) {
//	recon_val = udiffReconstructDCBuff(&patch_cfh, &src_cfh, NULL, &dcbuff);
    }
    v1printf("reconstruction return=%ld\n", recon_val);
    v1printf("reconstructing target file based off of dcbuff commands...\n");
    reconstructFile(&dcbuff, &src_cfh, &out_cfh);
    if(BDELTA_FORMAT==patch_id) {
	if(ctell(&out_cfh, CSEEK_ABS) < dcbuff.ver_size) {
	    unsigned char buff[512];
	    unsigned long to_write;
	    to_write = dcbuff.ver_size - ctell(&out_cfh, CSEEK_ABS);
	    memset(buff, 0, 512);
	    while(to_write > 0) {
		cwrite(&out_cfh, buff, MIN(to_write, 512));
		to_write -= MIN(to_write, 512);
	    }	    
	}
    }
    v1printf("reconstruction done.\n");
    DCBufferFree(&dcbuff);
    cclose(&out_cfh);
    cclose(&src_cfh);
    cclose(&patch_cfh);
    return 0;
}

