/*
  Copyright (C) 2003-2004 Brian Harring

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
#ifndef _HEADER_PRIMES
#define _HEADER_PRIMES 1

typedef struct _PRIME_CTX {
	unsigned int *base_primes;
	unsigned long prime_count;
	unsigned long array_size;
} PRIME_CTX;

int init_primes(PRIME_CTX *ctx);
void free_primes(PRIME_CTX *ctx);
unsigned long get_nearest_prime(PRIME_CTX *ctx, unsigned long near);
#endif
