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
#ifndef _HEADER_COMMAND_LIST
#define _HEADER_COMMAND_LIST 1

#include "defs.h"

typedef struct {
    off_u64 offset;
    off_u64 len;
} DCLoc;

typedef struct {
    off_u64 src_pos;
    off_u64 ver_pos;
    off_u64 len;
} DCLoc_match;

typedef struct {
    DCLoc		*command;
    DCLoc_match		*full_command;
    unsigned char	*src_id;
    unsigned long	com_count;
    unsigned long	com_size;
} command_list;

int
CL_init(command_list *cl, unsigned char full, unsigned long size, unsigned char store_src_ids);

void
CL_free(command_list *cl);

int
CL_add_command(command_list *cl, off_u64 src_pos, off_u64 len, unsigned char src_id);

int
CL_add_full_command(command_list *cl, off_u64 src_pos, off_u64 len, off_u64 ver_pos, unsigned char src_id);

int
CL_resize(command_list *cl, unsigned long increment);

#endif

