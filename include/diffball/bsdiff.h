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
#ifndef _HEADER_BSDIFF
#define _HEADER_BSDIFF 1

#define BSDIFF4_MAGIC				"BSDIFF40"
#define BSDIFF3_MAGIC				"BSDIFF30"
#define BSDIFF_QS_MAGIC				"QSUFDIFF"
#define BSDIFF_MAGIC_LEN 8
#include <diffball/diff-algs.h>
#include <cfile.h>

unsigned int check_bsdiff_magic(cfile *patchf);
signed int bsdiffEncodeDCBuffer(CommandBuffer *buffer, cfile *ver_cfh, 
		cfile *out_cfh);
signed int bsdiffReconstructDCBuff(DCB_SRC_ID src_id, cfile *patchf, CommandBuffer *dcbuff);

#endif
