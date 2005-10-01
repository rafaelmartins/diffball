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
#ifndef _HEADER_DCBUFFER_CFH_FUNCS
#define _HEADER_DCBUFFER_CFH_FUNCS 1

#include <cfile.h>
#include <dcbuffer.h>

unsigned long default_dcb_src_cfh_read_func(u_dcb_src usrc, unsigned long src_pos, 
	unsigned char *buf, unsigned long len);

unsigned long default_dcb_src_cfh_copy_func(DCommand *dc, cfile *out_cfh);

#endif

