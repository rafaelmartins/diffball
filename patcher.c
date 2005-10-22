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
#include <cfile.h>
#include <diffball/formats.h>
#include <diffball/defs.h>
#include "options.h"
#include <diffball/errors.h>
#include <diffball/dcbuffer.h>
#include <diffball/api.h>

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
	int out_fh;
	cfile src_cfh, out_cfh;
	cfile patch_cfh[256];
	cfile *patch_array[256];
	unsigned long x;
	char  *src_name = NULL;
	char  *out_name = NULL;
	unsigned long patch_count;
	char  **patch_name;
	unsigned long format_id;
	signed long int recon_val=0;
//	unsigned int out_compressor = 0;
	unsigned int output_to_stdout = 0;
	char  *patch_format = NULL;
	int optr = 0, err;
	unsigned long reconst_size = 0xffff;
	
	#define DUMP_USAGE(exit_code) \
		print_usage("patcher", "src_file patch(es) [trg_file|or to stdout]", help_opts, exit_code);

	while((optr = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch(optr) {
		case OVERSION:
			print_version("patcher");		exit(0);
		case OUSAGE:
		case OHELP:
			DUMP_USAGE(0);
		case OVERBOSE:
			global_verbosity++;				break;
		case OSTDOUT:
			output_to_stdout = 1;		break;
		case 'f':
			patch_format = optarg;		break;
/*		case OBZIP2:
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
*/		case 'b':
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
	if((src_name=(char *)get_next_arg(argc, argv))==NULL) {
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
		err=copen(&patch_cfh[x], patch_name[x], AUTODETECT_COMPRESSOR, CFILE_RONLY);
		check_return2(err,"copen of patch")
		patch_array[x] = &patch_cfh[x];
	}

	v1printf("verbosity level(%u)\n", global_verbosity);

	if((err=copen(&src_cfh, src_name, AUTODETECT_COMPRESSOR, CFILE_RONLY)) != 0) {
		v0printf("error opening source file '%s': %i\n", src_name, err);
		exit(EXIT_FAILURE);
	}

	if((err=copen(&out_cfh, out_name, NO_COMPRESSOR, CFILE_WONLY|CFILE_NEW)) != 0) {
		v0printf("error opening output file, exitting %i\n", err);
		exit(EXIT_FAILURE);
	}

	if(patch_format != NULL) {
		format_id = check_for_format(patch_format, strlen(patch_format));
		if(format_id == 0) {
			v0printf("desired forced patch format '%s' is unknown\n", patch_format);
			exit(EXIT_FAILURE);
		}
	} else {
		format_id = 0;
	}
	recon_val = simple_reconstruct(&src_cfh, patch_array, patch_count, &out_cfh, format_id, reconst_size);
	cclose(&out_cfh);
	if(recon_val != 0) {
		if (!output_to_stdout) {
			unlink(out_name);
		}
	}
	cclose(&src_cfh);
	for(x=0; x < patch_count; x++) {
		cclose(&patch_cfh[x]);
	}
	return recon_val;
}

