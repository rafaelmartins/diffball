/*
  Copyright (C) 2003-2005 Brian Harring

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

static struct option long_opts[] = {
    STD_LONG_OPTIONS,
    FORMAT_LONG_OPTION("patch-format", 'f'),
    FORMAT_LONG_OPTION("max-buffer", 'b'),
    END_LONG_OPTS
};

static struct usage_options help_opts[] = {
    STD_HELP_OPTIONS,
    FORMAT_HELP_OPTION("patch-format", 'f', "Override patch auto-identification"),
    FORMAT_HELP_OPTION("max-buffer", 'b', "Override the default 128KB buffer max"),
    USAGE_FLUFF("Normal usage is patcher src-file patch(s) reconstructed-file\n"
    "if you need to override the auto-identification (eg, you hit a bug), use -f.  Note this settings\n"
    "affects -all- used patches, so it's use should be limited to applying a single patch"),
    END_HELP_OPTS
};

static char short_opts[] = STD_SHORT_OPTIONS "f:b:";


int
main(int argc, char **argv)
{
    struct stat src_stat, patch_stat;
    int src_fh, out_fh;
    int patch_fh[256];
    cfile src_cfh, out_cfh;
    cfile patch_cfh[256];
    CommandBuffer dcbuff[2];
    unsigned long x;
    signed int src_id;
    signed int err;
    char  *src_name = NULL;
    char  *out_name = NULL;
    unsigned long patch_count;
    unsigned char reorder_commands = 0;
    unsigned char bufferless = 1;
    char  **patch_name;
    unsigned long int patch_id[256];
    signed long int recon_val=0;
    unsigned int out_compressor = 0;
    unsigned int output_to_stdout = 0;
    unsigned int global_use_md5 = 0;
    char  *patch_format = NULL;
    int optr = 0;
    unsigned long reconst_size = 0xffff;
    
    #define DUMP_USAGE(exit_code) \
	print_usage("patcher", "src_file patch(es) [trg_file|or to stdout]", help_opts, exit_code);

    while((optr = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
	switch(optr) {
	case OVERSION:
	    print_version("patcher");	exit(0);
	case OUSAGE:
	case OHELP:
	    DUMP_USAGE(0);
	case OVERBOSE:
	    global_verbosity++;		break;
	case OSTDOUT:
	    output_to_stdout = 1;	break;
	case 'f':
	    patch_format = optarg;	break;
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
	case 'b':
	    reconst_size = atol(optarg);
	    if(reconst_size > 0x4000000 || reconst_size == 0) {
		v0printf("requested buffer size %lu isn't sane.  Must be greater then 0, and less then %lu\n", 
		     reconst_size,  0x4000000L);
		exit(EXIT_USAGE);
	    }
	    break;
	default:
	    v0printf("unknown option %s\n", argv[optind]);
	    DUMP_USAGE(EXIT_USAGE);
	
	}
    }
    if( ((src_name=(char *)get_next_arg(argc, argv))==NULL) || 
	(stat(src_name, &src_stat))) {
	if(src_name) {
	    v0printf("Must specify an existing source file!- %s not found\n", src_name);
	    exit(EXIT_USAGE);
	}
	DUMP_USAGE(EXIT_USAGE);
    } else if (optind >= argc) {
	v0printf("Must specify a patch file!\n")
	DUMP_USAGE(EXIT_USAGE);
    }
    patch_count = argc - optind;
    patch_name = optind + argv;
    if(output_to_stdout) {
	out_fh = 1;
	if(patch_count == 0) {
	    v0printf("Must specify an existing patch file!\n");
	    DUMP_USAGE(EXIT_USAGE);
	}
    } else {
    	if(patch_count == 1) {
	    v0printf("Must specify a name for the reconstructed file!\n");
	    DUMP_USAGE(EXIT_USAGE);
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
	   exit(EXIT_FAILURE);
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

    if(patch_count != 1 || reorder_commands != 0) {
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
    if(src_cfh.compressor_type != NO_COMPRESSOR) {
    	reorder_commands = 1;
    }
    for(x=0; x < patch_count; x++) {
        if(x == patch_count - 1 && reorder_commands == 0 && bufferless) {
    	    if((out_fh = open(out_name, O_RDWR | O_TRUNC | O_CREAT, 0644))==-1) {
		v0printf( "error creating out file (open failed)\n");
    		exit(1);
    	    }
    	    if(copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WR)) {
		v0printf("error opening output file, exitting\n");
		exit(EXIT_FAILURE);
    	    }
            v1printf("not reordering, and bufferless is %u, going bufferless\n", bufferless);
	    if(x==0) {
	        err=DCB_no_buff_init(&dcbuff[0], 0, src_stat.st_size, 0, &out_cfh);
		check_return2(err, "DCBufferInit");
            	src_id = internal_DCB_register_cfh_src(&dcbuff[0], &src_cfh, NULL, NULL, DC_COPY, 0);
            	check_return(src_id, "internal_DCB_register_src", "unable to continue");
	    } else {
	    	err=DCB_no_buff_init(&dcbuff[x % 2], 0, dcbuff[(x - 1) % 2].ver_size, 0, &out_cfh);
		check_return2(err, "DCBufferInit");
    	    	src_id = DCB_register_dcb_src(dcbuff + ( x % 2), dcbuff + ((x -1) % 2));
            	check_return(src_id, "internal_DCB_register_src", "unable to continue");
	    }

	} else {
            if(x==0) {
    	    	err=DCB_full_init(&dcbuff[0], 4096, src_stat.st_size, 0);
		check_return2(err, "DCBufferInit");
            	src_id = internal_DCB_register_cfh_src(&dcbuff[0], &src_cfh, NULL, NULL, DC_COPY, 0);
            	check_return2(src_id, "DCB_register_cfh_src");
    	    } else {
    	    	err=DCB_full_init(&dcbuff[x % 2], 4096, dcbuff[(x - 1) % 2].ver_size , 0);
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
    	    v1printf(", commands=%ld\n", ((DCB_full *)dcbuff[x % 2].DCB)->cl.com_count);
	    v1printf("result was %lu commands\n", ((DCB_full *)dcbuff[x % 2].DCB)->cl.com_count);
    	} else {
    	    v1printf("\n");
    	}
    	if(recon_val) {
	    v0printf("error detected while processing patch- quitting\n");
	    print_error(recon_val);
	    exit(EXIT_FAILURE);
	}
	v1printf("versions size is %llu\n", (act_off_u64)dcbuff[x % 2].ver_size);
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
    	err = reconstructFile(&dcbuff[(patch_count - 1) % 2], &out_cfh,reorder_commands, reconst_size);
	check_return(err, "reconstructFile", "error detected while reconstructing file, quitting");
    	
    	v1printf("reconstruction completed successfully\n");
    } else {
    	v1printf("reconstruction completed successfully\n");
    }
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

