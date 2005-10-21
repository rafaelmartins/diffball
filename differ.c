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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cfile.h>
#include <diffball/diff-algs.h>
#include "string-misc.h"
#include <diffball/formats.h>
#include <diffball/defs.h>
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
	struct stat ref_stat, ver_stat;
	cfile out_cfh, ref_cfh, ver_cfh;
	int out_fh;
	EDCB_SRC_ID ref_id, ver_id;
	CommandBuffer buffer;

	int optr;
	char  *src_file = NULL;
	char  *trg_file = NULL;
	char  *patch_name = NULL;
	unsigned long patch_id = 0;
	signed long encode_result=0;
	unsigned long sample_rate=0;
	unsigned long seed_len = 0;
	unsigned long hash_size = 0;
	unsigned int patch_compressor = 0;
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
	if( ((src_file=(char *)get_next_arg(argc,argv)) == NULL) ||
		(stat(src_file, &ref_stat)) ) {
		if(src_file) {
			v0printf("Must specify an existing source file.\n");
			exit(EXIT_USAGE);
		}
		DUMP_USAGE(EXIT_USAGE);
	}
	if( ((trg_file=(char *)get_next_arg(argc, argv)) == NULL) ||
		(stat(trg_file, &ver_stat)) ) {
		if(trg_file) {
			v0printf("Must specify an existing target file.\n");
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
	copen(&ver_cfh, trg_file, NO_COMPRESSOR, CFILE_RONLY);
	copen(&ref_cfh, src_file, NO_COMPRESSOR, CFILE_RONLY);
	if(hash_size==0) {
		/* implement a better assessment based on mem and such */
		hash_size = MIN(DEFAULT_MAX_HASH_COUNT, ref_stat.st_size);
	}
	if(sample_rate==0) {
		/* implement a better assessment based on mem and such */
		sample_rate = COMPUTE_SAMPLE_RATE(hash_size, cfile_len(&ref_cfh));
	}
	if(seed_len==0) {
		seed_len = DEFAULT_SEED_LEN;
	}
	if(patch_format==NULL) {
		patch_id = DEFAULT_PATCH_ID;
	} else {
		patch_id = check_for_format(patch_format, strlen(patch_format));
		if(patch_id==0) {
			v0printf( "Unknown format '%s'\n", patch_format);
			exit(1);
		}
	}
	v1printf("using patch format %lu\n", patch_id);
	v1printf("using seed_len(%lu), sample_rate(%lu), hash_size(%lu)\n", 
		seed_len, sample_rate, hash_size);
	v1printf("verbosity level(%u)\n", global_verbosity);
	v1printf("initializing Command Buffer...\n");

	DCB_llm_init(&buffer, 4, ref_stat.st_size, ver_stat.st_size);
	v1printf("running multipass alg\n");
	ref_id = DCB_REGISTER_ADD_SRC(&buffer, &ver_cfh, NULL, 0);
	ver_id = DCB_REGISTER_COPY_SRC(&buffer, &ref_cfh, NULL, 0);
	MultiPassAlg(&buffer, &ref_cfh, ref_id, &ver_cfh, ver_id, hash_size);
	DCB_finalize(&buffer);
	DCB_test_total_copy_len(&buffer);
	v1printf("outputing patch...\n");
	if(copen_dup_fd(&out_cfh, out_fh, 0, 0, patch_compressor, CFILE_WONLY)) {
		v0printf("error allocing needed memory for output, exiting\n");
		exit(1);
	}
	if(GDIFF4_FORMAT == patch_id) {
		encode_result = gdiff4EncodeDCBuffer(&buffer, &out_cfh);
	} else if(GDIFF5_FORMAT == patch_id) {
		encode_result = gdiff5EncodeDCBuffer(&buffer, &out_cfh);
	} else if(BDIFF_FORMAT == patch_id) {
		encode_result = bdiffEncodeDCBuffer(&buffer, &out_cfh);
	} else if(SWITCHING_FORMAT == patch_id) {
		encode_result = switchingEncodeDCBuffer(&buffer, &out_cfh);
	} else if (BDELTA_FORMAT == patch_id) {
		encode_result = bdeltaEncodeDCBuffer(&buffer, &out_cfh);
	}
	v1printf("flushing and closing out file\n");
	cclose(&out_cfh);
	v1printf("encode_result=%ld\n", encode_result);
	v1printf("all commands processed? %u\n", DCB_commands_remain(&buffer)==0);
	v1printf("exiting\n");
	DCBufferFree(&buffer);
	v1printf("closing reference file\n");
	cclose(&ref_cfh);
	v1printf("closing version file\n");
	cclose(&ver_cfh);
	close(out_fh);
	return 0;
}
