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
#ifndef _HEADER_FDTU
#define _HEADER_FDTU 1
#include "cfile.h"

#define FDTU_MAGIC_LEN		3
#define FDTU_MAGIC		"DTU"
#define FDTU_VERSION_LEN	2
#define FDTU_MAGIC_V4		0x04
#define FDTU_MAGIC_V3		0x03

unsigned int check_fdtu_magic(cfile *patchf);
signed int fdtuEncodeDCBuffer(CommandBuffer *buffer, cfile *out_cfh);
signed int fdtuReconstructDCBuff(cfile *ref_cfh, cfile *patchf, CommandBuffer *dcbuff);


#endif
