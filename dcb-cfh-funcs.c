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
#include "cfile.h"
#include "dcbuffer.h"

unsigned long
default_dcb_src_cfh_read_func(u_dcb_src usrc, unsigned long src_pos, 
    unsigned char *buf, unsigned len)
{
    if(src_pos != cseek(usrc.cfh, src_pos, CSEEK_FSTART)) {
	return 0;
    }
    return cread(usrc.cfh, buf, len);
}

unsigned long 
default_dcb_src_cfh_copy_func(DCommand *dc, cfile *out_cfh)
{
//    return copy_cfile_block(out_cfh, dc->src_dcb->srcs[dc->src_id].cfh, 
//	(unsigned long)dc->data.src_pos, dc->data.len);
    return copy_cfile_block(out_cfh, dc->dcb_src->src_ptr.cfh, 
	(unsigned long)dc->data.src_pos, dc->data.len);
}

