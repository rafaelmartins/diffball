/*
  Copyright (C) 2003-2004 Brian Harring

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
#include "defs.h"
#include "options.h"
#include <stdlib.h>
#include <stdio.h>
#include <popt.h>

void 
usage(const char *prog, poptContext p_opt, int exitcode, const char *error, const char *addl)
{
    print_version(prog);
    poptPrintUsage(p_opt, stdout, 0);
    if(error) {
	if(addl) {
	    fprintf(stdout,"%s: %s\n", error,addl);
	} else {
	    fprintf(stdout,"%s\n", error);
	}
    }
    fprintf(stdout, "\n");
    exit(exitcode);
}

void
print_help(const char *prog, poptContext con)
{
    print_version(prog);
    poptPrintHelp(con,stdout,0);
    fprintf(stdout, "\n");
    exit(0);
}

void
print_version(const char *prog)
{
    fprintf(stdout,"diffball version %s, program %s (C) 2003-2004 Brian Harring\n", VERSION, prog);
    fprintf(stdout,"http://diffball.sourceforge.net\n");
    fprintf(stdout,"THIS SOFTWARE COMES WITH ABSOLUTELY NO WARRANTY! USE AT YOUR OWN RISK!\n");
    fprintf(stdout,"Report bugs to <bdharring@wisc.edu>\n\n");
}

