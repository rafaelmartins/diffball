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

static unsigned int PRIMES[] = {977,1163,1607,523,1117,269,163,1201,331,541,293,1129,1423,601,257,503,1459,7,863,419,47,1301,769,887,1429,73,563,373,673,173,241,1051,1049,37,1061,1433,197,739,499,313,409,23,19,1279,263,1481,983,17,5,1613,1231,97,463,811,79,1283,1087,1259,823,191,1181,29,211,1327,83,337,1567,61,797,593,877,937,149,227,1483,1597,709,353,449,991,743,277,137,607,1487,1151,1277,1093,761,53,401,13,1019,1033,307,1091,433,1223,229,179,859,31,1583,619,1319,1609,751,613,1367,59,1511,397,1451,1297,311,1373,947,467,131,1447,1499,1009,239,647,829,431,599,773,1039,677,251,41,1291,151,43,193,577,883,1307,881,631,443,953,1493,1021,1289,521,809,1523,1543,1303,283,1579,733,1619,929,659,1427,1063,1471,1097,971,127,1193,421,1531,617,719,1123,1559,1399,389,491,1229,1109,103,757,487,1601,557,89,1213,1217,67,787,661,1571,653,919,271,691,1013,1553,1321,911,367,101,641,157,1621,1489,587,281,727,317,839,11,1031,853,547,1381,1171,383,821,199,1549,3,109,347,1237,139,71,701,907,113,1409,1249,857,1453,479,509,571,167,643,457,233,1361,1069,1187,223,1103,569,941,379,1153,439,683,349,997,827,181,359,1439,967,461,107};

//{3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,409,419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509,521,523,541,547,557,563,569,571,577,587,593,599,601,607,613,617,619,631,641,643,647,653,659,661,673,677,683,691,701,709,719,727,733,739,743,751,757,761,769,773,787,797,809,811,821,823,827,829,839,853,857,859,863,877,881,883,887,907,911,919,929,937,941,947,953,967,971,977,983,991,997,1009,1013,1019,1021,1031,1033,1039,1049,1051,1061,1063,1069,1087,1091,1093,1097,1103,1109,1117,1123,1129,1151,1153,1163,1171,1181,1187,1193,1201,1213,1217,1223,1229,1231,1237,1249,1259,1277,1279,1283,1289,1291,1297,1301,1303,1307,1319,1321,1327,1361,1367,1373,1381,1399,1409,1423,1427,1429,1433,1439,1447,1451,1453,1459,1471,1481,1483,1487,1489,1493,1499,1511,1523,1531,1543,1549,1553,1559,1567,1571,1579,1583,1597,1601,1607,1609,1613,1619,1621};
//statis unsigned int POS[] 
//={5,11,17,23,31,41,47,59,67,73,83,97,103,109,127,137};

/* ========================================================================= */

void init_adler32_seed(struct adler32_seed *ads, unsigned int seed_len,
	unsigned int multi) {
	ads->s1 = ads->s2 = ads->tail = 0;
	ads->seed_len = seed_len;
	ads->multi = multi;
	ads->parity=0;
	if((ads->last_seed = (unsigned int *)malloc(seed_len*sizeof(int)))==NULL) {\
		//printf("shite, error allocing needed memory\n");
		abort();
	}
	if((ads->seed_chars = (unsigned char *)malloc(seed_len))==NULL) {
		abort();
	}
}

void update_adler32_seed(struct adler32_seed *ads, unsigned char *buff, unsigned int len) {
	unsigned int x;
	if(len==ads->seed_len) {
		//printf("computing seed fully\n");
		ads->s1 = ads->s2 = ads->tail =0;
		for(x=0; x < ads->seed_len; x++) {
			ads->s1 += ads->multi * PRIMES[buff[x]];
			ads->s2 += ads->s1;
			ads->last_seed[x] = PRIMES[buff[x]];
			ads->seed_chars[x] = buff[x];
		}
		ads->tail = 0;
	} else {
		for(x=0; x < len; x++){
			ads->s1 = ads->s1 - (ads->multi * ads->last_seed[ads->tail]) + 
				(ads->multi * PRIMES[buff[x]]);
			ads->s2 = ads->s2 - (ads->multi * ads->seed_len * 
				ads->last_seed[ads->tail]) + ads->s1;
			//ads->last_seed[ads->tail] = buff[x%4];
			//if((x%4)==3) {
				ads->seed_chars[ads->tail] = buff[x];
				ads->last_seed[ads->tail] = PRIMES[buff[x]];
				ads->tail = (ads->tail + 1) % ads->seed_len;
			//}
		}
	}
}

unsigned long get_checksum(struct adler32_seed *ads) {
	unsigned long chksum;
	unsigned int parity=0, x=0;
	for(x=0; x < ads->seed_len; x++) {
		parity = (ads->seed_chars[x] + parity) % 2;
	}
	chksum = (unsigned long)((ads->s2 << 16) | (ads->s1 & 0xffff));
	return (unsigned long)(chksum + parity);
	//return (unsigned long)((ads->s2 << 16) | (ads->s1 & 0xffff));
}

