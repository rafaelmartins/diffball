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

unsigned int verbosity = 0;
unsigned int out_compressor = 0;
unsigned int output_to_stdout = 0;
unsigned int use_md5 = 0;
char  *patch_format;

    /*lname,sname, info, ptr, val, desc, args */
/*struct poptOption options[] = {
    {"version",		'V', POPT_ARG_NONE, 0, OVERSION,0, 0},
    {"verbose",		'v', POPT_ARG_NONE, 0, OVERBOSE,0, 0},
    {"format",		'f', POPT_ARG_STRING, &patch_format,  0,0,  0},
    {"stdout",		'c', POPT_ARG_NONE, &output_to_stdout, 0,0, 0},
    {"ignore-md5",	'm', POPT_ARG_NONE, &use_md5, 0,0, 0},
    {"bzip2-compress",	'j', POPT_ARG_NONE, 0, OBZIP2,0, 0},
    {"gzip-compress",	'z', POPT_ARG_NONE, 0, OGZIP, 0,0},*/
struct poptOption options[] = {
    STD_OPTIONS(output_to_stdout),
    FORMAT_OPTIONS("patch-format", 'f', patch_format),
    MD5_OPTION(use_md5),
    POPT_TABLEEND
};

int
main(int argc, char **argv)
{
    struct stat src_stat, patch_stat;
    int src_fh, patch_fh, out_fh;
    unsigned int offset_type;
    cfile src_cfh, patch_cfh, out_cfh;
    CommandBuffer dcbuff;
    poptContext p_opt;

    signed long optr;
    char  *src_name;
    char  *out_name;
    char  *patch_name;

    p_opt = poptGetContext("patcher", argc, (const char **)argv, options, 0);
    while((optr=poptGetNextOpt(p_opt)) != -1) {
	if(optr < -1) {
	    usage(p_opt, 1, poptBadOption(p_opt,
		POPT_BADOPTION_NOALIAS), poptStrerror(optr));
	}
	switch(optr) {
	case OVERSION:
	    // print version.
	    exit(0);
	case OVERBOSE:
	    verbosity++;
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
	if((out_name = poptGetArg(p_opt))==NULL)
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
    v1printf("verbosity level(%u)\n", verbosity);
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
    copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY);
//    offset_type = ENCODING_OFFSET_START;
    offset_type = ENCODING_OFFSET_DC_POS;
	DCBufferInit(&dcbuff, 2000000, 0, 0);
//	switchingReconstructDCBuff(&patch_cfh, &dcbuff, offset_type);
   	gdiffReconstructDCBuff(&patch_cfh, &dcbuff, offset_type, 4);
//	rawReconstructDCBuff(&patch_cfh, &dcbuff, offset_type);
//	bdiffReconstructDCBuff(&patch_cfh, &dcbuff);
//	xdelta1ReconstructDCBuff(&patch_cfh, &dcbuff, 1);
//	bdeltaReconstructDCBuff(&patch_cfh, &dcbuff);
//	udiffReconstructDCBuff(&patch_cfh, &src_cfh, NULL, &dcbuff);
   	v1printf("reconstructing target file based off of dcbuff commands...\n");
   	reconstructFile(&dcbuff, &src_cfh, &patch_cfh, &out_cfh);
   	v1printf("reconstruction done.\n");
	DCBufferFree(&dcbuff);
	cclose(&out_cfh);
	cclose(&src_cfh);
	cclose(&patch_cfh);
	return 0;
}

