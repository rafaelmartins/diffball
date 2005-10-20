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
#ifndef _HEADER_XDELTA1
#define _HEADER_XDELTA1 1
#include <diffball/dcbuffer.h>
#include <cfile.h>

#define XD_COMPRESSED_FLAG		0x8

#define XDELTA_110_MAGIC "%XDZ004%"
#define XDELTA_104_MAGIC "%XDZ003%"
#define XDELTA_100_MAGIC "%XDZ002%"
#define XDELTA_020_MAGIC "%XDZ001%"
#define XDELTA_018_MAGIC "%XDZ000%"
#define XDELTA_014_MAGIC "%XDELTA%"
#define XDELTA_MAGIC_LEN 8

#define XD_INDEX_COPY 1
#define XD_INDEX_ADD 0

unsigned int check_xdelta1_magic(cfile *patchf);
signed int xdelta1EncodeDCBuffer(CommandBuffer *buffer, 
	unsigned int version, cfile *out_cfh);
signed int xdelta1ReconstructDCBuff(DCB_SRC_ID src_id, cfile *patchf, CommandBuffer *dcbuff, 
	unsigned int version);


#endif
