/*
  Copyright (C) 2003-2004 Brian Harring

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
#include "apply-patch.h"
#include "formats.h"
#include "defs.h"
#include "options.h"
#include "errors.h"
#include "dcbuffer.h"

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
    int src_fh, out_fh;
    int patch_fh[256];
    cfile src_cfh, out_cfh;
    cfile patch_cfh[256];
    CommandBuffer dcbuff[2];
    poptContext p_opt;
    unsigned long x;
    signed int src_id;
    signed int err;
    signed long optr;
    char  *src_name;
    char  *out_name;
    unsigned long patch_count;
    unsigned char reorder_commands = 0;
    unsigned char bufferless = 1;
    char  **patch_name, **p;
    unsigned long int patch_id[256];
    signed long int recon_val=0;

    p_opt = poptGetContext("patcher", argc, (const char **)argv, options, 0);
    while((optr=poptGetNextOpt(p_opt)) != -1) {
	if(optr < -1) {
	    usage("patcher", p_opt, 1, poptBadOption(p_opt,
		POPT_BADOPTION_NOALIAS), poptStrerror(optr));
	}
	switch(optr) {
	case OVERSION:
	    print_version("patcher");
	    exit(0);
	case OUSAGE:
	    usage("patcher", p_opt, 0, NULL, NULL);
	    break;
	case OVERBOSE:
	    global_verbosity++;
	    break;
	case OHELP:
	    print_help("patcher", p_opt);
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
	(stat(src_name, &src_stat))) {
	usage("patcher", p_opt, 1, "Must specify an existing source file.", 0);
    } else if((patch_name = (char **)poptGetArgs(p_opt)) == NULL) {
    	usage("patcher", p_opt, 1, "Must specify an existing patch file.", 0);
    }
    patch_count = 0;
    for(p=patch_name; *p != NULL; p++)
    	patch_count++;
    if(output_to_stdout) {
	out_fh = 1;
	if(patch_count == 0) {
	    usage("patcher", p_opt, 1, "Must specify an existing patch file.", 0);
	}
    } else {
    	if(patch_count == 1) {
    	    usage("patcher", p_opt, 1, "Must specify a name for the reconstructed file.", 0);
    	}
	out_name = patch_name[patch_count - 1];
	patch_name[patch_count] = NULL;
	patch_count--;
    }

    /* currently, unwilling to do bufferless for more then one patch.  overlay patches are the main 
       concern; it shouldn't be hard completing the support, just no motivation currently :) */

    for(x=0; x < patch_count; x++) {
        if(stat(patch_name[x], &patch_stat)) {
	   v0printf("error stat'ing patch file '%s'\n", patch_name[x]);
           usage("patcher", p_opt, 1, "Must specify an existing patch file.", 0);
        }
    	if((patch_fh[x] = open(patch_name[x], O_RDONLY,0))==-1) {
	    v0printf( "error opening patch file '%s'\n", patch_name[x]);
	    exit(EXIT_FAILURE);
    	}
        v1printf("patch_fh size=%lu\n", (unsigned long)patch_stat.st_size);
    	err=copen(&patch_cfh[x], patch_fh[x], 0, patch_stat.st_size, 
	    AUTODETECT_COMPRESSOR, CFILE_RONLY);
	check_return2(err,"copen of patch")
	/* if compression is noticed, reorganize. */
	if(patch_cfh[x].compressor_type != NO_COMPRESSOR)
	    reorder_commands = 1;

    	if(patch_format==NULL) {
	    patch_id[x] = identify_format(&patch_cfh[x]);
	    if(patch_id[x]==0) {
	    	v0printf( "Couldn't identify the patch format for patch %lu, aborting\n", patch_id[x]);
	        exit(EXIT_FAILURE);
	    } else if((patch_id[x] & 0xffff)==1) {
	    	v0printf( "Unsupported format version\n");
	    	exit(EXIT_FAILURE);
	    }
	    patch_id[x] >>=16;
    	} else {
	    patch_id[x] = check_for_format(patch_format, strlen(patch_format));
	    if(patch_id[x]==0) {
	    	v0printf( "Unknown format '%s'\n", patch_format);
	        exit(1);
	    }
	}
    	v1printf("patch_type=%lu\n", patch_id[x]);
    	cseek(&patch_cfh[x], 0, CSEEK_FSTART);
    }

    if(patch_count -1) {
    	bufferless = 0;
    	v1printf("disabling bufferless, patch_count(%lu) != 1\n", patch_count);
    } else {
    	v1printf("enabling bufferless, patch_count(%lu) != 1\n", patch_count);
    	bufferless = 1;
    }

    v1printf("verbosity level(%u)\n", global_verbosity);
    if ((src_fh = open(src_name, O_RDONLY,0)) == -1) {
	v0printf("error opening source file '%s'\n", src_name);
	exit(EXIT_FAILURE);
    }

    v1printf("src_fh size=%lu\n", (unsigned long)src_stat.st_size);
    copen(&src_cfh, src_fh, 0, src_stat.st_size, AUTODETECT_COMPRESSOR, 
	CFILE_RONLY);

    for(x=0; x < patch_count; x++) {
        if(x == patch_count - 1 && reorder_commands == 0 && bufferless) {
            v1printf("not reordering, and bufferless is %u, going bufferless\n", bufferless);
	    if(x==0) {
	        err=DCBufferInit(&dcbuff[0], 0, src_stat.st_size, 0, DCBUFFER_BUFFERLESS_TYPE);
		check_return2(err, "DCBufferInit");
            	src_id = internal_DCB_register_cfh_src(&dcbuff[0], &src_cfh, NULL, NULL, DC_COPY, 0);
            	check_return(src_id, "internal_DCB_register_src", "unable to continue");
	    } else {
	    	err=DCBufferInit(&dcbuff[x % 2], 0, dcbuff[(x - 1) % 2].ver_size, 0, DCBUFFER_BUFFERLESS_TYPE);
		check_return2(err, "DCBufferInit");
    	    	src_id = DCB_register_dcb_src(dcbuff + ( x % 2), dcbuff + ((x -1) % 2));
            	check_return(src_id, "internal_DCB_register_src", "unable to continue");
	    }

    	    if((out_fh = open(out_name, O_RDWR | O_TRUNC | O_CREAT, 0644))==-1) {
		v0printf( "error creating out file (open failed)\n");
    		exit(1);
    	    }
    	    if(copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WR)) {
		v0printf("error opening output file, exitting\n");
		exit(EXIT_FAILURE);
    	    }
	    DCB_register_out_cfh(&dcbuff[x % 2], &out_cfh);
	} else {
            if(x==0) {
    	    	err=DCBufferInit(&dcbuff[0], 4096, src_stat.st_size, 0, 
	    	    DCBUFFER_FULL_TYPE);
		check_return2(err, "DCBufferInit");
            	src_id = internal_DCB_register_cfh_src(&dcbuff[0], &src_cfh, NULL, NULL, DC_COPY, 0);
            	check_return2(src_id, "DCB_register_cfh_src");
    	    } else {
    	    	err=DCBufferInit(&dcbuff[x % 2], 4096, dcbuff[(x - 1) % 2].ver_size , 0, 
	    	    DCBUFFER_FULL_TYPE);
	    	check_return2(err, "DCBufferInit");
    	    	src_id = DCB_register_dcb_src(dcbuff + ( x % 2), dcbuff + ((x -1) % 2));
    	    	check_return2(src_id, "DCB_register_dcb_src");
    	    }
	}

    	if(SWITCHING_FORMAT == patch_id[x]) {
	    recon_val = switchingReconstructDCBuff(src_id, &patch_cfh[x], &dcbuff[x % 2]);
    	} else if(GDIFF4_FORMAT == patch_id[x]) {
	    recon_val = gdiff4ReconstructDCBuff(src_id, &patch_cfh[x], &dcbuff[x % 2]);
    	} else if(GDIFF5_FORMAT == patch_id[x]) {
	    recon_val = gdiff5ReconstructDCBuff(src_id, &patch_cfh[x], &dcbuff[x % 2]);
    	} else if(BDIFF_FORMAT == patch_id[x]) {
	    recon_val = bdiffReconstructDCBuff(src_id, &patch_cfh[x], &dcbuff[x % 2]);
    	} else if(XDELTA1_FORMAT == patch_id[x]) {
	    recon_val = xdelta1ReconstructDCBuff(src_id, &patch_cfh[x], &dcbuff[x % 2], 1);

	    /* this could be adjusted a bit, since with xdelta it's optional to compress the patch
	       in such a case, the decision for reordering shouldn't be strictly controlled here */
	    if(patch_count > 1)
	    	reorder_commands = 1;

    	} else if(BDELTA_FORMAT == patch_id[x]) {
	    recon_val = bdeltaReconstructDCBuff(src_id, &patch_cfh[x], &dcbuff[x % 2]);
    	} else if(BSDIFF_FORMAT == patch_id[x]) {
	    recon_val = bsdiffReconstructDCBuff(src_id, &patch_cfh[x], &dcbuff[x % 2]);

	    /* compressed format- if more then one patch, reorder
	       besides that, currently multiple bsdiff patches are supported only via 
	       read_seq_write_rand.  needs fixing later on */
	    if(patch_count > 1)
	    	reorder_commands = 1;

    	} else if(FDTU_FORMAT == patch_id[x]) {
	    recon_val = fdtuReconstructDCBuff(src_id, &patch_cfh[x], &dcbuff[x % 2]);

	    /* wrapped xdelta format, same reasoning applies */
	    if(patch_count > 1)
	    	reorder_commands = 1;

//    	} else if(UDIFF_FORMAT == patch_id[x]) {
//	    recon_val = udiffReconstructDCBuff(src_id, &patch_cfh[x], &src_cfh, NULL, &dcbuff[x % 2]);
    	}

    	v1printf("reconstruction return=%ld", recon_val);
	if(DCBUFFER_FULL_TYPE == dcbuff[x % 2].DCBtype) {
    	    v1printf(", commands=%ld\n", dcbuff[x % 2].DCB.full.cl.com_count);
	    v1printf("result was %lu commands\n", dcbuff[x % 2].DCB.full.cl.com_count);
    	} else {
    	    v1printf("\n");
    	}
    	if(recon_val) {
	    v0printf("error detected while processing patch- quitting\n");
	    print_error(recon_val);
	    exit(EXIT_FAILURE);
	}
	v1printf("versions size is %lu\n", dcbuff[x % 2].ver_size);
	if(x) {
	    DCBufferFree(&dcbuff[(x - 1) % 2]);
    	}
    }
    v1printf("applied %lu patches\n", patch_count);

    if(! bufferless) {
	if((out_fh = open(out_name, O_RDWR | O_TRUNC | O_CREAT, 0644))==-1) {
	    v0printf( "error creating out file (open failed)\n");
    	    exit(1);
    	}
        if(copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WR)) {
	    v0printf("error opening output file, exitting\n");
	    exit(EXIT_FAILURE);
    	}

    	v1printf("reordering commands? %u\n", reorder_commands);
    	v1printf("reconstructing target file based off of dcbuff commands...\n");
    	err = reconstructFile(&dcbuff[(patch_count - 1) % 2], &out_cfh,reorder_commands);
	check_return(err, "reconstructFile", "error detected while reconstructing file, quitting");
    	
    	v1printf("reconstruction completed successfully\n");
    } else {
    	v1printf("reconstruction completed successfully\n");
    }
    poptFreeContext(p_opt);
    DCBufferFree(&dcbuff[(patch_count - 1) % 2]);
    cclose(&out_cfh);
    cclose(&src_cfh);
    close(src_fh);
    for(x=0; x < patch_count; x++) {
    	cclose(&patch_cfh[x]);
    	close(patch_fh[x]);
    }
    close(out_fh);
    return 0;
}

