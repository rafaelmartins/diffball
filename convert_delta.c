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

struct option long_opts[] = {
    STD_LONG_OPTIONS,
    FORMAT_LONG_OPTION("src-format",'s'),
    FORMAT_LONG_OPTION("trg-format",'t'),
    END_LONG_OPTS
};

struct usage_options help_opts[] = {
    STD_HELP_OPTIONS,
    FORMAT_HELP_OPTION("src-format", 's', "override auto-identification and specify source patches format"),
    FORMAT_HELP_OPTION("trg-format", 't', "override default and specify new patches format"),
    USAGE_FLUFF("convert_delta either expects 2 args (src patch, and new patches name), or just the source\n"
    "patches name if the option to dump to stdout has been given\n"
    "examples\n"
    "this would convert from the (auto-identified) xdelta format, to the default switching format\n"
    "convert_delta kde.xdelta kde.patch\n\n"
    "this would convert from the (auto-identified) xdelta format, to the gdiff4 format\n"
    "convert_delta kde.xdelta -t gdiff4 kde.patch\n"),
    END_HELP_OPTS
};

char short_opts[] = STD_SHORT_OPTIONS "s:t:";

int 
main(int argc, char **argv)
{
    int in_fh, out_fh;
    struct stat in_stat;
    CommandBuffer dcbuff;
    cfile in_cfh, out_cfh;
    int optr;
    int src_id;
    char *src_file, *trg_file;
    unsigned long int src_format_id, trg_format_id=0;
    signed long recon_val=0, encode_result=0;
    unsigned int patch_compressor = 0;
    unsigned int output_to_stdout = 0;
    unsigned int global_use_md5 = 0;
    char *src_format = NULL, *trg_format = NULL;

    #define DUMP_USAGE(exit_code) \
	print_usage("convert_delta", "src_patch -t format [new_patch|or to stdout]", help_opts, exit_code)

    while((optr = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
	switch(optr) {
	case OVERSION:
	    print_version("convert_delta");	exit(0);
	case OUSAGE:
	case OHELP:
	    DUMP_USAGE(0);
	case OVERBOSE:
	    global_verbosity++;
	    break;
	case 'f':
	    src_format = optarg;	break;
	case 't':
	    trg_format = optarg;	break;
	case OSTDOUT:
	    output_to_stdout = 1;	break;
	default:
	    v0printf("unknown option %s\n", argv[optind]);
	    DUMP_USAGE(EXIT_USAGE);

/*	case OBZIP2:
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
*/
	}
    }
    if( ((src_file=(char *)get_next_arg(argc, argv))==NULL) || 
	(stat(src_file, &in_stat))) {
	if(src_file) {
	    v0printf("Must specify an existing patch\n");
	    exit(EXIT_USAGE);
	}
	DUMP_USAGE(EXIT_USAGE);
    }
    if(output_to_stdout) {
	out_fh = 1;
    } else {
	if((trg_file = (char *)get_next_arg(argc, argv))==NULL)
	    DUMP_USAGE(EXIT_USAGE);
	if((out_fh = open(trg_file, O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1){
	    v0printf( "error creating output file '%s'\n", trg_file);
	    exit(1);
	}
    }
    if(NULL!= get_next_arg(argc,argv)) {
	DUMP_USAGE(EXIT_USAGE);
    }
    if((in_fh = open(src_file, O_RDONLY, 0))==-1) {
	v0printf( "error opening patch '%s'\n", src_file);
	exit(EXIT_FAILURE);
    }
    copen(&in_cfh, in_fh, 0, in_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    if(src_format == NULL) {
	src_format_id = identify_format(&in_cfh);
	if(src_format_id==0) {
	    v0printf("Couldn't identify the patch format, aborting\n");
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

    if(trg_format==NULL) {
	v0printf("new files format is required\n");
	DUMP_USAGE(EXIT_USAGE);
    } else {
	trg_format_id = check_for_format(trg_format, strlen(trg_format));
	if(trg_format_id==0) {
	    v0printf( "Unknown format '%s'\n", trg_format);
	    exit(1);
	}
    }
    DCB_full_init(&dcbuff, 4096,0,0);
    src_id = internal_DCB_register_cfh_src(&dcbuff, NULL, &bail_if_called_func, 
    	&bail_if_called_func, DC_COPY, 0);
    if(SWITCHING_FORMAT == src_format_id) {
        recon_val = switchingReconstructDCBuff(src_id, &in_cfh, &dcbuff);
    } else if(GDIFF4_FORMAT == src_format_id) {
        recon_val = gdiff4ReconstructDCBuff(src_id, &in_cfh, &dcbuff);
    } else if(GDIFF5_FORMAT == src_format_id) {
        recon_val = gdiff5ReconstructDCBuff(src_id, &in_cfh, &dcbuff);       
    } else if(BDIFF_FORMAT == src_format_id) {
        recon_val = bdiffReconstructDCBuff(src_id, &in_cfh, &dcbuff);       
    } else if(XDELTA1_FORMAT == src_format_id) {
        recon_val = xdelta1ReconstructDCBuff(src_id, &in_cfh, &dcbuff, 1);
    } else if(BDELTA_FORMAT == src_format_id) {
        recon_val = bdeltaReconstructDCBuff(src_id, &in_cfh, &dcbuff);
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
    v1printf("there were %lu commands\n", ((DCB_full *)dcbuff.DCB)->cl.com_count);
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
