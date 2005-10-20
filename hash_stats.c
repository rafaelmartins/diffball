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
#include "cfile.h"
#include "diff-algs.h"
//#include "bit-functions.h"
#include "string-misc.h"
#include "formats.h"
#include "cfile.h"
#include "defs.h"
#include "options.h"

unsigned int global_verbosity=0;
unsigned long sample_rate=0;
unsigned long seed_len = 0;
unsigned long hash_size = 0;
unsigned int patch_compressor = 0;
unsigned int patch_to_stdout = 0;
char  *patch_format;

struct poptOption options[] = {
	DIFF_OPTIONS(seed_len, sample_rate,hash_size),
	POPT_TABLEEND
};

int main(int argc, char **argv)
{
	struct stat ref_stat;
	cfile ref_cfh;
	int ref_fh;
	//char *src, *trg;
	RefHash rhash;
	poptContext p_opt;

	signed long optr;
	char  *src_file;
	p_opt = poptGetContext("differ", argc, (const char **)argv, options, 0);
	while((optr=poptGetNextOpt(p_opt)) != -1) {
		if(optr < -1) {
			usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS), 
				poptStrerror(optr));
		}
		switch(optr) {
		case OVERSION:
			print_version("differ");
		case OVERBOSE:
			global_verbosity++;
			break;
/*		case OBZIP2:
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
			break;*/
		}
	}
	if( ((src_file=(char *)poptGetArg(p_opt))==NULL) || 
		(stat(src_file, &ref_stat))) 
		usage(p_opt, 1, "Must specify an existing source file.",NULL);
	if (NULL!=poptGetArgs(p_opt)) {
		usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS),
		"unknown option");
	}
	poptFreeContext(p_opt);
	if ((ref_fh = open(src_file, O_RDONLY,0)) == -1) {
		v0printf( "error opening src_file\n");
		exit(EXIT_FAILURE);
	}
	copen(&ref_cfh, ref_fh, 0, ref_stat.st_size, NO_COMPRESSOR, CFILE_RONLY |
		CFILE_BUFFER_ALL);
	if(hash_size==0) {
		/* implement a better assessment based on mem and such */
		hash_size = MIN(DEFAULT_MAX_HASH_COUNT, ref_stat.st_size);
	}
	if(sample_rate==0) {
		/* implement a better assessment based on mem and such */
//		sample_rate = 1;
		sample_rate = COMPUTE_SAMPLE_RATE(hash_size, cfile_len(&ref_cfh));
	}
	unsigned long seedx=512;
	v1printf("using seed_len(%lu), sample_rate(%lu), hash_size(%lu)\n", 
		seed_len, sample_rate, hash_size);
	v1printf("verbosity level(%u)\n", global_verbosity);
	v0printf("beginning hash stats run.\n");
	for(seedx= (seed_len == 0 ? 512 : seed_len); seedx >= 
		(seed_len == 0 ? 16 : seed_len); seedx -= 16) {
		v0printf("seed_len(%lu)\n", seedx);
		v1printf("initing hash\n");
		init_RefHash(&rhash, &ref_cfh, seedx, sample_rate, hash_size, 
			RH_MOD_HASH);
		print_RefHash_stats(&rhash);
		free_RefHash(&rhash);
	}
	v1printf("closing ref file\n");
	cclose(&ref_cfh);
	return 0;
}
