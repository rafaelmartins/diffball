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
#include <string.h>
#include <errno.h>
#include "cfile.h"
#include "dcbuffer.h"
#include "apply-patch.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))


void 
reconstructFile(CommandBuffer *dcbuff, cfile *src_cfh, cfile *delta_cfh, 
    cfile *out_cfh)
{
//    unsigned long x, tmp, count;
    unsigned long count;
    count = DCBufferReset(dcbuff);
    while(count--) {
	if(current_command_type(dcbuff)==DC_COPY) {
	    printf("copy command, offset(%lu), len(%lu)\n",
		dcbuff->lb_tail->offset, dcbuff->lb_tail->len);
		cseek(src_cfh, dcbuff->lb_tail->offset, CSEEK_FSTART);
	    if(dcbuff->lb_tail->len != 
		copy_cfile_block(out_cfh, src_cfh, dcbuff->lb_tail->offset,
		dcbuff->lb_tail->len))
		abort();
	} else {
	    printf("add command, offset(%lu), len(%lu)\n", 
		dcbuff->lb_tail->offset, dcbuff->lb_tail->len);
	    if(dcbuff->lb_tail->len !=
		copy_cfile_block(out_cfh, delta_cfh, dcbuff->lb_tail->offset, 
		dcbuff->lb_tail->len))
		abort();
	}
	DCBufferIncr(dcbuff);
    }
}
	
	
