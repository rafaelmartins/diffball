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
#ifndef _HEADER_PRIMES
#define _HEADER_PRIMES 1

struct prime_ctx {
	unsigned int *base_primes;
	unsigned long prime_count;
	unsigned long array_size;
};

void init_primes(struct prime_ctx *ctx);
void free_primes(struct prime_ctx *ctx);
unsigned long get_nearest_prime(struct prime_ctx *ctx, unsigned long near);
#endif
