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
#include <unistd.h>
#include <cfile.h>
#include <fcntl.h>
#include "string-misc.h"
#include <diffball/formats.h>
#include <diffball/defs.h>
#include <diffball/api.h>
#include "options.h"
#include <diffball/errors.h>

unsigned int global_verbosity=0;

char  *patch_format;

struct option long_opts[] = {
	STD_LONG_OPTIONS,
	DIFF_LONG_OPTIONS,
	FORMAT_LONG_OPTION("patch-format", 'f'),
	END_LONG_OPTS
};

struct usage_options help_opts[] = {
	STD_HELP_OPTIONS,
	DIFF_HELP_OPTIONS,
	FORMAT_HELP_OPTION("patch-format", 'f', "format to output the patch in"),
	USAGE_FLUFF("differ expects 3 args- source, target, name for the patch\n"
	"if output to stdout is enabled, only 2 args required- source, target\n"
	"Example usage: differ older-version newerer-version upgrade-patch"),
	END_HELP_OPTS
};

char short_opts[] = STD_SHORT_OPTIONS DIFF_SHORT_OPTIONS "f:";

int main(int argc, char **argv)
{
	cfile out_cfh, ref_cfh, ver_cfh;
	int out_fh;

	int optr;
	char  *src_file = NULL;
	char  *trg_file = NULL;
	char  *patch_name = NULL;
	unsigned long patch_id = 0;
	signed long encode_result=0;
	int err;
	unsigned long sample_rate=0;
	unsigned long seed_len = 0;
	unsigned long hash_size = 0;
//	unsigned int patch_compressor = 0;
	unsigned int patch_to_stdout = 0;

	#define DUMP_USAGE(exit_code)		\
		print_usage("differ", "src_file trg_file [patch_file|or to stdout]", help_opts, exit_code);

	while((optr = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch(optr) {
		case OVERSION:
			print_version("differ");		exit(0);
		case OUSAGE:
		case OHELP:
			DUMP_USAGE(0);
		case OVERBOSE:
			global_verbosity++;				break;
		case OSAMPLE:
			sample_rate = atol(optarg);
			if(sample_rate == 0 || sample_rate > MAX_SAMPLE_RATE) DUMP_USAGE(EXIT_USAGE);
			break;
		case OSEED:
			seed_len = atol(optarg);
			if(seed_len == 0 || seed_len > MAX_SEED_LEN) DUMP_USAGE(EXIT_USAGE);
			break;
		case OHASH:
			hash_size = atol(optarg);
			if(hash_size == 0 || hash_size > MAX_HASH_SIZE) DUMP_USAGE(EXIT_USAGE);
			break;
		case OSTDOUT:
			patch_to_stdout = 1;		break;
		case 'f':
			patch_format = optarg;		break;
		default:
			v0printf("invalid arg- %s\n", argv[optind]);
			DUMP_USAGE(EXIT_USAGE);
		}
	}
	err = 0;
	if( ((src_file = (char *)get_next_arg(argc,argv)) == NULL) ||
		(err = copen(&ref_cfh, src_file, NO_COMPRESSOR, CFILE_RONLY)) != 0) {
		if(src_file) {
			if(err == MEM_ERROR) {
				v0printf("alloc failure for src_file\n");
			} else {
				v0printf("Must specify an existing source file.\n");
			}
			exit(EXIT_USAGE);
		}
		DUMP_USAGE(EXIT_USAGE);
	}
	err = 0;
	if( ((trg_file=(char *)get_next_arg(argc, argv)) == NULL) ||
		(err = copen(&ver_cfh, trg_file, NO_COMPRESSOR, CFILE_RONLY)) != 0) {
		if(trg_file) {
			if(err == MEM_ERROR) {
				v0printf("alloc failure for trg_file\n");
			} else {
				v0printf("Must specify an existing target file.\n");
			}
			exit(EXIT_USAGE);
		}
		DUMP_USAGE(EXIT_USAGE);
	}
	if(patch_to_stdout != 0) {
		out_fh = 1;
	} else {
		if((patch_name = (char *)get_next_arg(argc, argv)) == NULL)
			DUMP_USAGE(EXIT_USAGE);
		if((out_fh = open(patch_name, O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
			v0printf( "error creating patch file '%s' (open failed)\n", patch_name);
			exit(1);
		}
	}
	if (NULL!=get_next_arg(argc, argv)) {
		DUMP_USAGE(EXIT_USAGE);
	}

	if(patch_format==NULL) {
		patch_id = DEFAULT_PATCH_ID;
	} else {
		patch_id = check_for_format(patch_format, strlen(patch_format));
		if(patch_id==0) {
			v0printf( "Unknown format '%s'\n", patch_format);
			exit(EXIT_FAILURE);
		}
	}
	if(copen_dup_fd(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR /* patch_compressor */, CFILE_WONLY)) {
		v0printf("error allocing needed memory for output, exiting\n");
		exit(EXIT_FAILURE);
	}

	v1printf("using patch format %lu\n", patch_id);
	v1printf("using seed_len(%lu), sample_rate(%lu), hash_size(%lu)\n", 
		seed_len, sample_rate, hash_size);
	v1printf("verbosity level(%u)\n", global_verbosity);
	v1printf("initializing Command Buffer...\n");

	encode_result = simple_difference(&ref_cfh, &ver_cfh, &out_cfh, patch_id, seed_len, sample_rate, hash_size);
	v1printf("flushing and closing out file\n");
	cclose(&out_cfh);
	close(out_fh);
	if(err) {
		if(! patch_to_stdout) {
			unlink(patch_name);
		}
	}
	v1printf("encode_result=%ld\n", encode_result);
	v1printf("exiting\n");
	v1printf("closing reference file\n");
	cclose(&ref_cfh);
	v1printf("closing version file\n");
	cclose(&ver_cfh);
	close(out_fh);
	check_return2(encode_result, "encoding result was nonzero")
	return 0;
}
