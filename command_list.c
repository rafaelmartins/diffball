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
#include <stdlib.h>
#include "command_list.h"

int
CL_init(command_list *cl, unsigned char full, unsigned long size, unsigned char store_src_ids)
{
	if(size == 0)
			size = 512;
	
	if(store_src_ids) {
		if((cl->src_id = (unsigned char *)malloc(sizeof(unsigned char *) * size))==NULL) {
				return MEM_ERROR;
			}
	} else {
			cl->src_id = NULL;
	}
	
	if(full) {
			cl->command = NULL;
			cl->full_command = (DCLoc_match *)malloc(sizeof(DCLoc_match) * size);
	} else {
			cl->full_command = NULL;
			cl->command = (DCLoc *)malloc(sizeof(DCLoc) * size);
	}
	
	if(cl->command == NULL && cl->full_command == NULL) {
			if(cl->src_id)
				free(cl->src_id);
			return MEM_ERROR;
	}
	
	cl->com_count = 0;
	cl->com_size = size;
	return 0;
}

void
CL_free(command_list *cl)
{
	if(cl->full_command)
			free(cl->full_command);
	else
			free(cl->command);
	if(cl->src_id)
		free(cl->src_id);
	cl->src_id = NULL;
	cl->full_command = NULL;
	cl->command = NULL;
	cl->com_count = 0;
	cl->com_size = 0;
}

int
CL_add_command(command_list *cl, off_u64 src_pos, off_u64 len, unsigned char src_id)
{
	if(cl->com_count == cl->com_size) {
			if(CL_resize(cl,0))
				return MEM_ERROR;
	}
	cl->command[cl->com_count].offset = src_pos;
	cl->command[cl->com_count].len = len;
	if(cl->src_id)
			cl->src_id[cl->com_count] = src_id;
	cl->com_count++;
	return 0;
}

int
CL_add_full_command(command_list *cl, off_u64 src_pos, off_u64 len, off_u64 ver_pos, unsigned char src_id)
{
	if(cl->com_count == cl->com_size) {
			if(CL_resize(cl,0))
				return MEM_ERROR;
	}
	cl->full_command[cl->com_count].src_pos = src_pos;
	cl->full_command[cl->com_count].len = len;
	cl->full_command[cl->com_count].ver_pos = ver_pos;
	if(cl->src_id)
			cl->src_id[cl->com_count] = src_id;
	cl->com_count++;
	return 0;
}

int
CL_resize(command_list *cl, unsigned long increment)
{
	unsigned long size;
	void *ptr;
	
	if(increment)
			size = cl->com_size + increment;
	else
			size = cl->com_size * 2;
	
	if(cl->full_command) {
			ptr = realloc(cl->full_command, sizeof(DCLoc_match) * size);
	} else
			ptr = realloc(cl->command, sizeof(DCLoc) * size);
	if(ptr == NULL)
			return MEM_ERROR;
	
	if(cl->full_command)
			cl->full_command = (DCLoc_match *)ptr;
	else
			cl->command = (DCLoc *)ptr;
	if(cl->src_id) {
			ptr = realloc(cl->src_id, sizeof(unsigned char) * size);
			if(ptr == NULL)
				return MEM_ERROR;
			cl->src_id = (unsigned char *)ptr;
	}
	cl->com_size = size;
	return 0;
}

