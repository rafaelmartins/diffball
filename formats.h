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
#ifndef _HEADER_FORMATS
#define _HEADER_FORMATS 1
#include "cfile.h"
#include "bdelta.h"
#include "bdiff.h"
#include "gdiff.h"
#include "switching.h"
#include "xdelta1.h"

#define GDIFF4_FORMAT		0x1
#define GDIFF5_FORMAT		0x2
#define BDIFF_FORMAT		0x3
#define BSDIFF3_FORMAT		0x4
#define BSDIFF4_FORMAT		0x5
#define XDELTA1_FORMAT		0x6
#define SWITCHING_FORMAT 	0x7
#define BDELTA_FORMAT		0x8
#define GNUDIFF_FORMAT		0x9

#define DEFAULT_PATCH_ID	SWITCHING_FORMAT


unsigned long int check_for_format(char *format_name, unsigned int len);
unsigned long int identify_format(cfile *patchf);

#endif

