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
#ifndef _HEADER_BDELTA
#define _HEADER_BDELTA 1
#include "dcbuffer.h"
#include "cfile.h"

#define BDELTA_MAGIC		"BDT"
#define BDELTA_MAGIC_LEN	3
#define BDELTA_VERSION		0x1
#define BDELTA_VERSION_LEN	2

unsigned int check_bdelta_magic(cfile *patchf);
signed int bdeltaEncodeDCBuffer(CommandBuffer *dcbuff, 
    	cfile *ver_cfh, cfile *out_fh);
signed int bdeltaReconstructDCBuff(cfile *patchf, 
	CommandBuffer *dcbuff);


#endif
