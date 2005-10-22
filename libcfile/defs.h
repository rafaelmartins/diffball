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
#ifndef _HEADER_DEFS
#define _HEADER_DEFS 1

#include "config.h"
#include <errno.h>

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#define v0printf(expr...) fprintf(stderr, expr);

#ifdef DEV_VERSION
#include <assert.h>
#define eprintf(expr...)   abort(); fprintf(stderr, expr);
#else
#define assert(expr) ((void)0)
#define eprintf(expr...)   fprintf(stderr, expr);
#endif

#define v1printf(expr...)  if(cfile_verbosity>0){fprintf(stderr,expr);}
#define v2printf(expr...)  if(cfile_verbosity>1){fprintf(stderr,expr);}
#define v3printf(expr...)  if(cfile_verbosity>2){fprintf(stderr,expr);}
#define v4printf(expr...)  if(cfile_verbosity>3){fprintf(stderr,expr);}

#ifdef DEBUG_CFILE
#include <stdio.h>
#define dcprintf(fmt...) \
	fprintf(stderr, "%s: ",__FILE__);   \
	fprintf(stderr, fmt);
#else
#define dcprintf(expr...) ((void) 0);
#endif

#endif

