/*
  Copyright (C) 2003-2006 Brian Harring

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
#include <string.h>
#include <errno.h>
#include <diffball/defs.h>
#include "options.h"

void
print_usage(const char *prog, const char *usage_portion, struct usage_options *text, int exit_code) {

	struct usage_options *u_ptr;
	print_version(prog);
	u_ptr = text;
	unsigned int long_len = 0;
	while (1) {
		if(u_ptr->long_arg == NULL && u_ptr->description == NULL) 
			break;
		if(u_ptr->long_arg != NULL) {
			long_len = MAX(strlen(u_ptr->long_arg), long_len);
		}
		u_ptr++;
	}
	long_len=MIN(15, long_len);
	fprintf(stdout, "	usage: %s [flags] ", prog);
	if(usage_portion)
		fprintf(stdout, "%s", usage_portion);
	fprintf(stdout, "\n\n");
	u_ptr = text;  
	while (1) {
		if(u_ptr->long_arg == NULL && u_ptr->description == NULL)
			break;
		if(0 != u_ptr->short_arg) {
			fprintf(stdout, "  -%c ", u_ptr->short_arg);
			if(NULL != u_ptr->long_arg)
				fprintf(stdout, "--%-*s", long_len, u_ptr->long_arg);
			else
				fprintf(stdout, "  %*s", long_len, "");
			if(NULL != u_ptr->description)
				fprintf(stdout, " %s\n", u_ptr->description);
			else
				fprintf(stdout, "\n");
		} else if(NULL != u_ptr->long_arg) {
			fprintf(stdout, "	 --%-*s", long_len, u_ptr->long_arg);
			if(NULL != u_ptr->description)
				fprintf(stdout, " %s\n", u_ptr->description);
			else
				fprintf(stdout, "\n");
		} else if(u_ptr->description != NULL) {
			// description fluff
			fprintf(stdout, "\n%s\n", u_ptr->description);
		} else {
			// all opts exhausted.  end of usage struct aparently (else someone screwed up)
			break;
		}
		u_ptr++;
	}
	fprintf(stdout, "\n");
	exit(exit_code);
}
			

void
print_version(const char *prog)
{
	fprintf(stdout,"diffball version %s, program %s (C) 2003-2006 Brian Harring\n", VERSION, prog);
	fprintf(stdout,"http://code.google.com/diffball\n");
	fprintf(stdout,"THIS SOFTWARE COMES WITH ABSOLUTELY NO WARRANTY! USE AT YOUR OWN RISK!\n");
	fprintf(stdout,"Report bugs to <ferringb@gmail.com>\n\n");
}

char *
get_next_arg(int argc, char **argv) {
	if(argc > optind)
		return argv[optind++];
	return NULL;
}
