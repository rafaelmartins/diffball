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
/*#define GDIFF_MAGIC 0xd1ffd1ff
#define GDIFF_MAGIC_LEN 4
#define GDIFF_VER4 4
#define GDIFF_VER5 5
#define GDIFF_VER6 6
#define GDIFF_VER_LEN 1
*/
#include "diff-algs.h"
//#include "pdbuff.h"
#include "cfile.h"

signed int switchingEncodeDCBuffer(CommandBuffer *buffer, 
    unsigned int offset_type, cfile *ver_cfh, cfile *out_cfh);
signed int switchingReconstructDCBuff(cfile *patchf, CommandBuffer *dcbuff, 
    unsigned int offset_type);


#endif
