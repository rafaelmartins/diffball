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
/* as per the norm, modified version of zlib's adler32. 
   eg the code I've wroted, but the original alg/rolling chksum is 
   Mark Adler's baby....*/
   
/* adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */
#include <stdlib.h>
#include "adler32.h"


/* ========================================================================= */

void init_adler32_seed(struct adler32_seed *ads, unsigned int seed_len,
	unsigned int multi) {
	ads->s1 = ads->s2 = ads->tail = 0;
	ads->seed_len = seed_len;
	ads->multi = multi;
	if((ads->last_seed = (unsigned char *)malloc(seed_len))==NULL) {\
		//printf("shite, error allocing needed memory\n");
		abort();
	}
}

void update_adler32_seed(struct adler32_seed *ads, unsigned char *buff, unsigned int len) {
	unsigned int x;
	if(len==ads->seed_len) {
		//printf("computing seed fully\n");
		ads->s1 = ads->s2 = ads->tail =0;
		for(x=0; x < ads->seed_len; x++) {
			ads->s1 += ads->multi * buff[x];
			ads->s2 += ads->s1;
			ads->last_seed[x] = buff[x];
		}
		ads->tail = 0;
	} else {
		for(x=0; x < len; x++){
			ads->s1 = ads->s1 - (ads->multi * ads->last_seed[ads->tail]) + 
				(ads->multi * buff[x]);
			ads->s2 = ads->s2 - (ads->multi * ads->seed_len * 
				ads->last_seed[ads->tail]) + ads->s1;
			//ads->last_seed[ads->tail] = buff[x%4];
			//if((x%4)==3) {
				ads->last_seed[ads->tail] = buff[x];
				ads->tail = (ads->tail + 1) % ads->seed_len;
			//}
		}
	}
}


/*void update_adler32_seed(struct adler32_seed *ads, unsigned char *buff, unsigned int len) {
	unsigned int x;
	if(len==ads->seed_len) {
		//printf("computing seed fully\n");
		ads->s1 = ads->s2 = ads->tail =0;
		for(x=0; x < ads->seed_len; x++) {
			ads->s1 += buff[x];
			ads->s2 += ads->s1;
			ads->last_seed[x] = buff[x];
		}
		ads->tail = 0;
	} else {
		for(x=0; x < len; x++){
			ads->s1 = ads->s1 - ads->last_seed[ads->tail] + buff[x];
			ads->s2 = ads->s2 - (ads->seed_len * ads->last_seed[ads->tail]) + ads->s1;
			ads->last_seed[ads->tail] = buff[x];
			ads->tail = (ads->tail + 1) % ads->seed_len;
		}
	}
}*/

