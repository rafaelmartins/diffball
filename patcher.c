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
//    unsigned long patch_compressor_type;
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
	out_fh = 1;
    } else {
	if((out_name = (char *)poptGetArg(p_opt))==NULL)
	    usage(p_opt, 1, "Must specify a name for the out file.", 0);
	if((out_fh = open(out_name, O_RDWR | O_TRUNC | O_CREAT, 0644))==-1) {
	    v0printf( "error creating out file (open failed)\n");
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
	v0printf("error opening source file '%s'\n", src_name);
	exit(EXIT_FAILURE);
    }
    if((patch_fh = open(patch_name, O_RDONLY,0))==-1) {
	v0printf( "error opening patch file '%s'\n", patch_name);
	exit(EXIT_FAILURE);
    }
    v1printf("src_fh size=%lu\n", (unsigned long)src_stat.st_size);
    v1printf("patch_fh size=%lu\n", (unsigned long)patch_stat.st_size);
    copen(&src_cfh, src_fh, 0, src_stat.st_size, AUTODETECT_COMPRESSOR, 
	CFILE_RONLY);
    copen(&patch_cfh, patch_fh, 0, patch_stat.st_size, 
	AUTODETECT_COMPRESSOR, CFILE_RONLY);
    if(patch_format==NULL) {
	patch_id = identify_format(&patch_cfh);
	if(patch_id==0) {
	    v0printf( "Couldn't identify the patch format, aborting\n");
	    exit(EXIT_FAILURE);
	} else if((patch_id & 0xffff)==1) {
	    v0printf( "Unsupported format version\n");
	    exit(EXIT_FAILURE);
	}
	patch_id >>=16;
    } else {
	patch_id = check_for_format(patch_format, strlen(patch_format));
	if(patch_id==0) {
	    v0printf( "Unknown format '%s'\n", patch_format);
	    exit(1);
	}
    }
    v1printf("patch_type=%lu\n", patch_id);
    cseek(&patch_cfh, 0, CSEEK_FSTART);
    if(DCBufferInit(&dcbuff, 4096, src_stat.st_size, 0, 
	DCBUFFER_FULL_TYPE)) {
	v0printf("unable to alloc needed mem, exiting\n");
	abort();
    }
    if(SWITCHING_FORMAT == patch_id) {
	recon_val = switchingReconstructDCBuff(&src_cfh, &patch_cfh, &dcbuff);
    } else if(GDIFF4_FORMAT == patch_id) {
	recon_val = gdiff4ReconstructDCBuff(&src_cfh, &patch_cfh, &dcbuff);
    } else if(GDIFF5_FORMAT == patch_id) {
	recon_val = gdiff5ReconstructDCBuff(&src_cfh, &patch_cfh, &dcbuff);
    } else if(BDIFF_FORMAT == patch_id) {
	recon_val = bdiffReconstructDCBuff(&src_cfh, &patch_cfh, &dcbuff);
    } else if(XDELTA1_FORMAT == patch_id) {
	recon_val = xdelta1ReconstructDCBuff(&src_cfh, &patch_cfh, &dcbuff, 1);
    } else if(BDELTA_FORMAT == patch_id) {
	recon_val = bdeltaReconstructDCBuff(&src_cfh, &patch_cfh, &dcbuff);
    } else if(BSDIFF_FORMAT == patch_id) {
	recon_val = bsdiffReconstructDCBuff(&src_cfh, &patch_cfh, &dcbuff);
    } else if(FDTU_FORMAT == patch_id) {
	recon_val = fdtuReconstructDCBuff(&src_cfh, &patch_cfh, &dcbuff);
//    } else if(UDIFF_FORMAT == patch_id) {
//	recon_val = udiffReconstructDCBuff(&src_cfh, &patch_cfh, &src_cfh, NULL, &dcbuff);
    }
    v1printf("reconstruction return=%ld\n", recon_val);
    v1printf("reconstructing target file based off of dcbuff commands...\n");
    if(recon_val) {
	v0printf("error detected while reading patch- quitting\n");
    } else {
//	DCBUFFER_REGISTER_COPY_SRC(&dcbuff, &src_cfh, NULL, 0);
	if(copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WR)) {
	    v0printf("error opening output file, exitting\n");
	} else if(reconstructFile(&dcbuff, &out_cfh,1)) {
	    v0printf("error detected while reconstructing file, quitting\n");
	    //remove the file here.
	}
	cclose(&out_cfh);
	v1printf("reconstruction completed successfully\n");
    }
    DCBufferFree(&dcbuff);
    cclose(&src_cfh);
    cclose(&patch_cfh);
    close(src_fh);
    close(patch_fh);
    close(out_fh);
    return 0;
}

