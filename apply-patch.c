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
reconstructFile(CommandBuffer *dcbuff, cfile *out_cfh)
{
    DCommand dc;
    assert(DCBUFFER_FULL_TYPE == dcbuff->DCBtype);
    DCBufferReset(dcbuff);
    while(DCB_commands_remain(dcbuff)) {
	DCB_get_next_command(dcbuff, &dc);
	if(DC_COPY == dc.type) {
	    v2printf("copy command, offset(%lu), len(%lu)\n",
		dc.loc.offset, dc.loc.len);
		//cseek(src_cfh, DCBF_cur_off(dcbuff), CSEEK_FSTART);
//	    if(dc.loc.len != copy_cfile_block(out_cfh, src_cfh, dc.loc.offset,
//		dc.loc.len))
	    if(dc.loc.len != copyDCB_copy_src(dcbuff, &dc, out_cfh)) {
		abort();
	    }
	} else {
	    v2printf("add command, offset(%lu), len(%lu)\n", 
		dc.loc.offset, dc.loc.len);
//	    if(dc.loc.len != copy_cfile_block(out_cfh, dcbuff->add_cfh, 
//		dc.loc.offset, dc.loc.len))
	    if(dc.loc.len != copyDCB_add_src(dcbuff, &dc, out_cfh)) {
		abort();
	    }
	}
    }
}
	
	
