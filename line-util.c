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
#include "defs.h"
#include "line-util.h"

unsigned long
skip_lines_forward(cfile *cfh, unsigned long n)
{
    cfile_window *cfw;
    unsigned long lines=0;
    cfw = expose_page(cfh);
    while(lines < n && cfw->end) {
	/* a compressed handle will have probs here... */
//	assert(cfw->offset + cfw->pos <= cfh->data_total_len);
	assert(cfw->pos <= cfw->end);
	if(cfw->buff[cfw->pos]=='\n') {
	    lines++;
	}
	cfw->pos++;
	if(cfw->pos==cfw->end) {
	   cfw = next_page(cfh);
	}
    }
    return lines;
}

unsigned long
skip_lines_backward(cfile *cfh, unsigned long n)
{
    unsigned long lines=0;
    cfile_window *cfw;
    cfw=expose_page(cfh);
    while(lines <= n && cfw->end != 0) {
	if(cfw->buff[cfw->pos]=='\n') {
	    lines++;
	}
	cfw->pos--;
	if(cfw->pos==0)
	   cfw = prev_page(cfh);
    }
    return (lines ? lines -1 : 0);
}

