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
#include <stdlib.h>
#include "bit-functions.h"

inline unsigned int 
unsignedBitsNeeded(unsigned long int y)
{
    unsigned int x=1;
    if (y == 0) {
	return 0;
    }
    while((y = y >>1) > 0)
	x++;
    return x;    
}

inline unsigned int 
signedBitsNeeded(signed long int y)
{
    return unsignedBitsNeeded(abs(y)) + 1;
}

inline unsigned int 
unsignedBytesNeeded(unsigned long int y)
{
    unsigned int x;
    if (y == 0) {
	return 0;
    }
    x=unsignedBitsNeeded(y);
    x= (x/8) + (x % 8 ? 1 : 0);
    return x;
}

inline unsigned int 
signedBytesNeeded(signed long int y)
{
    unsigned int x;
    x=signedBitsNeeded(abs(y));
    x= (x/8) + (x % 8 ? 1 : 0);
    return x;
}

unsigned long 
readUBytesBE(const unsigned char *buff, unsigned int l)
{
    const unsigned char *p;
    unsigned long num=0;
    for(p = (unsigned const char*)buff; p - buff < l; p++)
	num = (num << 8) | *p;
    return (unsigned long)num;
}

unsigned long 
readUBytesLE(const unsigned char *buff, unsigned int l)
{
    unsigned long num=0;
    for(; l > 0; l--)
	num = (num << 8) | buff[l-1];
    return (unsigned long)num;
}

/*
signed long 
readSBytesLE(const unsigned char *buff, unsigned int l)
{
    unsigned long num = 0;
    num |= (buff[l-1] & 0x7f);
    for(; l > 1; l--)
	num |= (num << 8) + buff[l -1];

    num = *buff & 0x7f;  //strip the leading bit.
    for(p = buff -l -1; p != buff; p--) 
	num = (num << 8) + *p;
    return (signed long)(num * (*buff & 0x80 ? -1 : 1));
}
*/

signed long 
readSBytesBE(const unsigned char *buff, unsigned int l)
{
    unsigned long num;
    unsigned const char *p;
    num = *buff & 0x7f;  //strpi the leading bit.
    for(p = buff + 1; p - buff < l; p++) {
	num = (num << 8) + *p;
    }
    return (signed long)(num * (*buff & 0x80 ? -1 : 1));
}

unsigned int 
writeUBytesBE(unsigned char *buff, unsigned long value, unsigned int l)
{
    unsigned int x;
    for(x=0; x < l; x++)
	buff[x] = (value >> (l - 1 -x)*8) & 0xff;
    if(l > 4 && (value >> (l * 8)) > 0) 
	return 1;
    return 0;
}

unsigned int 
writeUBytesLE(unsigned char *buff, unsigned long value, unsigned int l)
{
    unsigned int x;
    for(x=0; l > 0; l--, x++) 
	buff[x] = ((value >> (x * 8)) & 0xff);
    if(l != 4 && (value >> (l * 8)) > 0)
	return 1;
    return 0;
}

unsigned int 
writeSBytesBE(unsigned char *buff, signed long value, unsigned int l)
{
    if(writeUBytesBE(buff, (unsigned long)abs(value), l)!=0) {
	return 1;
    } else if((buff[0] & 0x80) != 0) {
	return 1;
    } if(value < 0) 
	buff[0] |= 0x80;
    return 0;
}

unsigned int 
writeSBytesLE(unsigned char *buff, signed long value, unsigned int l)
{
    if(writeUBytesLE(buff, (unsigned long)abs(value), l)!=0)
	return 1;
    else if((buff[0] & 0x80) != 0)
	return 1;
    if(value < 0)
	buff[0] |= 0x80;
    return 0;
}

unsigned int 
writeSBitsBE(unsigned char *out_buff, signed long value, unsigned int bit_count)
{
	unsigned int start=0;
	start = bit_count % 8;
    writeUBitsBE(out_buff, abs(value), bit_count);
    if(value < 0) {
		if(out_buff[0] & (1 << start))
	    	return 1; //num was too large.
		out_buff[0] |= (1 << start);
    } else if (out_buff[0] & (1 << start)) { //num was too large.
		return 1;
    }
    return 0;    
}

unsigned int 
writeUBitsBE(unsigned char *out_buff, unsigned long value, unsigned int bit_count)
{
    unsigned int start_bit, byte;
    signed int x;
    start_bit = bit_count % 8;
    for(x = bit_count - start_bit, byte=0; x >= 0; byte++, x-=8) 
	out_buff[byte] = (value >> x) & 0xff;
    return 0;
}
