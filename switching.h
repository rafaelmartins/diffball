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
#ifndef _HEADER_SWITCHING
#define _HEADER_SWITCHING 1
#include "cfile.h"

#define SWITCHING_MAGIC "%SWITCHD%"
#define SWITCHING_MAGIC_LEN 8
#define SWITCHING_VERSION 0x00
#define SWITCHING_VERSION_LEN 1

unsigned int check_switching_magic(cfile *patchf);
signed int switchingEncodeDCBuffer(CommandBuffer *buffer, 
    /*unsigned int offset_type,*/ cfile *ver_cfh, cfile *out_cfh);
signed int switchingReconstructDCBuff(cfile *patchf, CommandBuffer *dcbuff /*, 
    unsigned int offset_type*/);
#endif
