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
#include <cfile.h>

#define MIN(x,y) ((x) < (y) ? (x) : (y))

unsigned long 
copy_cfile_block(cfile *out_cfh, cfile *in_cfh, unsigned long in_offset, 
    unsigned long len) 
{
#define BUFF_SIZE 4096
    unsigned char buff[BUFF_SIZE];
    unsigned int lb;
    unsigned long bytes_wrote=0;;
    if(in_offset!=cseek(in_cfh, in_offset, CSEEK_FSTART))
	abort();
    while(len) {
	lb = MIN(BUFF_SIZE, len);
	if( (lb!=cread(in_cfh, buff, lb)) ||
	    (lb!=cwrite(out_cfh, buff, lb)) )
	    abort;
	len -= lb;
	bytes_wrote+=lb;
    }
    return bytes_wrote;
}

