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
#ifndef _HEADER_OPTIONS
#define _HEADER_OPTIONS 1


#include <getopt.h>

//move this. but to where?
#define EXIT_USAGE -2

//enum {OVERSION=100, OVERBOSE,OUSAGE,OHELP,OFORMAT,OSEED,OSAMPLE,OSTDOUT,OBZIP2,OGZIP};

struct usage_options {
	char		short_arg;
	char		*long_arg;
	char		*description;
};

#define USAGE_FLUFF(fluff)		\
{0, 0, fluff}

#define OVERSION		'V'
#define OVERBOSE		'v'
#define OUSAGE				'u'
#define OHELP				'h'
#define OSEED				'b'
#define OSAMPLE				's'
#define OHASH				'a'
#define OSTDOUT				'c'
#define OBZIP2				'j'
#define OGZIP				'z'

#define DIFF_SHORT_OPTIONS										\
"b:s:a:"

#define DIFF_LONG_OPTIONS										\
{"seed-len",		1, 0, OSEED},										\
{"sample-rate", 1, 0, OSAMPLE},										\
{"hash-size",		1, 0, OHASH}										\

#define DIFF_HELP_OPTIONS										\
{OSEED, "seed-len",		"set the seed len"},						\
{OSAMPLE, "sample-rate","set the sample rate"},						\
{OHASH,		"hash-size",		"set the hash size"}


#define STD_SHORT_OPTIONS										\
"Vvcuh"

/*
{"bzip2-compress",		'j', POPT_ARG_NONE,0, OBZIP2,0,0},		\
{"gzip-compress",		'z', POPT_ARG_NONE,0, OGZIP,0,0}
*/

#define STD_LONG_OPTIONS										\
{"version",				0, 0, OVERSION},						\
{"verbose",				0, 0, OVERBOSE},						\
{"to-stdout",				0, 0, OSTDOUT},								\
{"usage",				0, 0, OUSAGE},								\
{"help",				0, 0, OHELP}

#define STD_HELP_OPTIONS										\
{OVERSION, "version",		 "print version"},						\
{OVERBOSE, "verbose",		 "increase verbosity"},						\
{OSTDOUT, "to-stdout",		 "output to stdout"},						\
{OUSAGE, "usage",		 "give this help"},						\
{OHELP, "help",				"give this help"}


//note no FORMAT_SHORT_OPTION

#define FORMAT_LONG_OPTION(long, short)								 \
{long,		1, 0, short}

#define FORMAT_HELP_OPTION(long,short,description)						\
{short, long, description}

#define END_HELP_OPTS {0, NULL, NULL}
#define END_LONG_OPTS {0,0,0,0}


/*#define get_next_arg(argc, argv)								\
(((argc) > optind && (argv)[optind++]) || NULL)
*/
// refresher for those who're going wtf, optind is an external (ab)used by getopt
char *get_next_arg(int argc, char **argv);
void print_version(const char *prog);
void print_usage(const char *prog, const char *usage_portion, struct usage_options *textq, int exit_code);


#endif

