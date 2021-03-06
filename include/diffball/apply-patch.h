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
#ifndef _HEADER_APPLY_PATCH
#define _HEADER_APPLY_PATCH 1
#include <diffball/dcbuffer.h>
#include <diffball/command_list.h>

int reconstructFile(CommandBuffer *dcbuff, cfile *out_cfh, 
	int reorder_for_seq_access, unsigned long max_buff_size);
int read_seq_write_rand(command_list *cl, DCB_registered_src *u_src, unsigned char is_overlay, cfile *out_cfh, 
	unsigned long buf_size);
#endif 
