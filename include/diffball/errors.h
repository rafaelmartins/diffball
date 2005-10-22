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
#ifndef _HEADER_ERRORS
#define _HEADER_ERRORS 1

#define check_return(err, msg, extra)											\
if(err) {																		\
	fprintf(stderr,"error detected in %s:%d\n", __FILE__,__LINE__);				\
	if(msg)																		\
			fprintf(stderr, "%s: ", (char *)(msg));								\
	print_error(err);															\
	if(extra)																	\
			fprintf(stderr, "%s\n", (char *)(extra));							\
	exit(err);																	\
}

#define check_return2(err, msg)													\
if(err) {																		\
	fprintf(stderr,"error detected in %s:%d\n", __FILE__,__LINE__);				\
	if(msg)																		\
			fprintf(stderr, "%s: ", (char *)(msg));								\
	print_error(err);															\
	exit(err);																	\
}

#define check_return_ret(err, level, msg)										\
if(err) {																		\
	if(global_verbosity > level) {												\
		if(msg)																	\
			fprintf(stderr, msg);												\
		print_error(err);														\
	}																			\
	return err;																	\
}

void print_error(int err);

#endif

