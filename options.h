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
#ifndef _HEADER_OPTIONS
#define _HEADER_OPTIONS 1
#include <popt.h>

enum {OVERSION=100, OVERBOSE,OFORMAT,OSEED,OSAMPLE,OSTDOUT,OBZIP2,OGZIP};
/*lname, sname, info, ptr, val, desc, args */
#define DIFF_OPTIONS(seed_len, sample_rate, hash_size)		\
{"seed-len",	'b', POPT_ARG_INT, &(seed_len),0,0,0},		\
{"sample-rate",	's', POPT_ARG_LONG, &(sample_rate),0,0,0},	\
{"hash-size",	'a', POPT_ARG_LONG, &(hash_size),0,0,0}

#define STD_OPTIONS(stdout)					\
{"version",		'V', POPT_ARG_NONE,0,OVERSION,0,0},	\
{"verbose",		'v', POPT_ARG_NONE,0,OVERBOSE,0,0},	\
{"to-stdout",		'c', POPT_ARG_NONE,&(stdout), 0,0,0},	\
{"bzip2-compress",	'j', POPT_ARG_NONE,0, OBZIP2,0,0},	\
{"gzip-compress",	'z', POPT_ARG_NONE,0, OGZIP,0,0}

#define FORMAT_OPTIONS(long, short, string)		\
{long, short, POPT_ARG_STRING, &string, 0,0,0}
#define MD5_OPTION(md5var)				\
{"ignore-md5",	'm',POPT_ARG_NONE, &(md5var),0,0,0}

void usage(poptContext p_opt, int exitcode, const char *error, 
    const char *addl);

#endif

