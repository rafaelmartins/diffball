/* as per the norm, modified version of zlib's adler32. */
/* adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/* @(#) $Id: adler32.c,v 1.2 2003/05/28 22:36:18 bharring Exp $ */

//#include "zlib.h"

//#define BASE 65521L /* largest prime smaller than 65536 */
//#define NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

/*#define DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);*/

/* ========================================================================= */
unsigned long adler32(char *buf, int len)
{
    unsigned long s1 = 0;
    unsigned long s2 = 0;
    //int k;

    //if (buf == NULL) return 1L;

    while (len > 0) {
        //k = len < NMAX ? len : NMAX;
        //len -= k;
        while (len-- > 0) {
            //DO16(buf);
	    s1+=*(buf++); s2 += s1;
        }
        /*if (k != 0) do {
            s1 += *buf++;
	    s2 += s1;
        } while (--k);*/
        //s1 %= BASE;
        //s2 %= BASE;
    }
    /* basically attempting to just mask it, rather then then do the whole moduls base thing.
       This *may* cause issues dependent on the checksum length... */
    return (s2 << 16) | (s1 & 0xffff);
}
