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
#ifndef _HEADER_ADLER32
#define _HEADER_ADLER32 1

typedef struct _ADLER32_SEED_CTX {
		unsigned int seed_len;
		unsigned int multi;
		unsigned long last_multi;
		unsigned int *last_seed;
		unsigned char *seed_chars;
		unsigned char *last_parity_bits;
		unsigned int parity;
		unsigned int tail;
		unsigned long s1;
		unsigned long s2;
} ADLER32_SEED_CTX;

int init_adler32_seed(ADLER32_SEED_CTX *ads, unsigned int seed_len, 
	unsigned int multi);
inline void update_adler32_seed(ADLER32_SEED_CTX *ads, unsigned char *buff,
	unsigned int len); 
unsigned long get_checksum(ADLER32_SEED_CTX *ads);
unsigned int free_adler32_seed(ADLER32_SEED_CTX *ads) ;

#endif

