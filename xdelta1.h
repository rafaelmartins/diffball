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
#ifndef _HEADER_XDELTA1
#define _HEADER_XDELTA1 1
#include "cfile.h"

#define XD_INDEX_COPY 1
#define XD_INDEX_ADD 0

signed int xdelta1EncodeDCBuffer(struct CommandBuffer *buffer, 
    unsigned int version, struct cfile *ver_cfh, struct cfile *out_cfh);
signed int xdelta1ReconstructDCBuff(struct cfile *patchf, struct CommandBuffer *dcbuff, 
    unsigned int version);


#endif
