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
#include "cfile.h"
#include "dcbuffer.h"
#include "formats.h"
#include "defs.h"
#include "options.h"

unsigned int verbosity = 0;
unsigned int patch_compressor = 0;
unsigned int output_to_stdout = 0;
unsigned int use_md5 = 0;
char *src_format, *trg_format;

//enum {OVERSION=100, OVERBOSE, OFORMAT, OBZIP2, OGZIP};

/*struct poptOption options[] = {
    {"version",		'V', POPT_ARG_NONE, 0, OVERSION,0, 0},
    {"verbose",		'v', POPT_ARG_NONE, 0, OVERBOSE,0, 0},
    {"format",		'f', POPT_ARG_STRING, &patch_format, 0,0,0},
    {"ignore-md5",	'm', POPT_ARG_NONE, &use_md5, 0,0, 0},
    {"stdout",		'c', POPT_ARG_NONE, &output_to_stdout, 0, 0, 1},
    {"bzip2-compress",	'j', POPT_ARG_NONE, 0, OBZIP2,0,0},
    {"gzip-compress",	'z', POPT_ARG_NONE, 0, OGZIP, 0, 0},*/
struct poptOption options[] = {
    STD_OPTIONS(output_to_stdout),
    FORMAT_OPTIONS("src-format", 's', src_format),
    FORMAT_OPTIONS("trg-format", 't', trg_format),
    MD5_OPTION(use_md5),
    POPT_TABLEEND
};

int 
main(int argc, char **argv)
{
    int in_fh, out_fh;
    struct stat in_stat, out_stat;
    unsigned int offset_type;
    CommandBuffer dcbuff;
    cfile in_cfh, out_cfh;
    poptContext p_opt;
    signed long optr;
    char *src_file, *out_file;

    p_opt = poptGetContext("convert_delta", argc, (const char **)argv, 
	options, 0);
    while((optr=poptGetNextOpt(p_opt)) != -1) {
	switch(optr) {
	case OVERSION:
	    // print version.
	    exit(0);
	case OVERBOSE:
	    verbosity++;
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
	out_fh = 0;
    } else {
	if((out_file = poptGetArg(p_opt))==NULL)
	    usage(p_opt, 1, "Must specify a name for the new patch.", NULL);
        if((out_fh = open(out_file, O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1){
	    fprintf(stderr, "error creating output file '%s'\n", out_file);
	    exit(1);
	}
    }
    if(NULL!= poptGetArgs(p_opt)) {
	usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS),
	    "unknown option");
    }
    poptFreeContext(p_opt);
    if((in_fh = open(src_file, O_RDONLY, 0))==-1) {
	fprintf(stderr, "error opening patch '%s'\n", src_file);
	exit(EXIT_FAILURE);
    }
    copen(&in_cfh, in_fh, 0, in_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY);
    DCBufferInit(&dcbuff, 1000000,0,0);
//    offset_type = ENCODING_OFFSET_START;
    offset_type = ENCODING_OFFSET_DC_POS;
    printf("reconstructing dcbuffer...\n");
//    switchingReconstructDCBuff(&in_cfh, &dcbuff, offset_type);
    gdiffReconstructDCBuff(&in_cfh, &dcbuff, offset_type, 4);
//    bdiffReconstructDCBuff(&in_cfh, &dcbuff);
    DCBufferCollapseAdds(&dcbuff);
    printf("outputing patch...\n");
//    gdiffEncodeDCBuffer(&dcbuff, offset_type, &in_cfh, &out_cfh);
    switchingEncodeDCBuffer(&dcbuff, &in_cfh, &out_cfh);
    printf("finished.\n");
    DCBufferFree(&dcbuff);
    cclose(&in_cfh);
    cclose(&out_cfh);
    return 0;
}
