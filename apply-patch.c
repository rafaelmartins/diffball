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
#include "defs.h"
#include "cfile.h"
#include "dcbuffer.h"
#include "apply-patch.h"
#include "defs.h"

void 
reconstructFile(CommandBuffer *dcbuff, cfile *src_cfh, cfile *out_cfh)
{
    assert(DCBUFFER_FULL_TYPE == dcbuff->DCBtype);
    unsigned long count;
    count = DCBufferReset(dcbuff);
    while(count--) {
	if(current_command_type(dcbuff)==DC_COPY) {
	    v2printf("copy command, offset(%lu), len(%lu)\n",
		DCBF_cur_off(dcbuff), DCBF_cur_len(dcbuff));
		//cseek(src_cfh, DCBF_cur_off(dcbuff), CSEEK_FSTART);
	    if(DCBF_cur_len(dcbuff) != 
		copy_cfile_block(out_cfh, src_cfh, DCBF_cur_off(dcbuff),
		DCBF_cur_len(dcbuff)))
		abort();
	} else {
	    v2printf("add command, offset(%lu), len(%lu)\n", 
		DCBF_cur_off(dcbuff), DCBF_cur_len(dcbuff));
	    if(DCBF_cur_len(dcbuff) !=
		copy_cfile_block(out_cfh, dcbuff->add_cfh, 
		    DCBF_cur_off(dcbuff), 
		DCBF_cur_len(dcbuff)))
		abort();
	}
	DCBufferIncr(dcbuff);
    }
}
	
	
