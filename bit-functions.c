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

inline int unsignedBitsNeeded(unsigned long int y)
{
    unsigned int x=1;
    if (y == 0) {
	//printf("no bytesneeded\n");
	return 0;
    }
    while((y = y >>1) > 0)
	x++;
    return x;    
}
inline int signedBitsNeeded(signed long int y)
{
    return unsignedBitsNeeded(abs(y)) + 1;
}

inline int unsignedBytesNeeded(unsigned long int y)
{
    unsigned int x;
    if (y == 0) {
	//printf("no bytesneeded\n");
	return 0;
    }
    x=unsignedBitsNeeded(y);
    x= (x/8) + (x % 8 ? 1 : 0);
    return x;
}
inline int signedBytesNeeded(signed long int y)
{
    unsigned int x;
    x=signedBitsNeeded(abs(y));
    x= (x/8) + (x % 8 ? 1 : 0);
    return x;
}

unsigned long readUnsignedBytes(const unsigned char *buff, unsigned char l)
{
    unsigned char *p;
    unsigned long num=0;
    for(p = (unsigned char*)buff; p - (unsigned char*)buff < l; p++) {
	num = (num << 8) | *p;
    }
    return (unsigned long)num;
}

signed long readSignedBytes(const unsigned char *buff, unsigned char l)
{
    unsigned long num;
    unsigned char *p;
    num = *buff & 0x7f;  //strpi the leading bit.
    for(p = (unsigned char *)buff + 1; p - buff < l; p++) {
	num = (num << 8) + *p;
    }
    return (signed long)(num * (*buff & 0x80 ? -1 : 1));
}

int convertSBytesChar(unsigned char *out_buff, signed long value, unsigned char byte_count)
{
    convertUBytesChar(out_buff, abs(value), byte_count);
    if(value < 0) {
	if(out_buff[0] & 0x80)
	    return -1; //num was too large.
	out_buff[0] |= 0x80;
    } else if (out_buff[0] & 0x80) { //num was too large.
	return -1;
    }
    return 0;    
}

int convertUBytesChar(unsigned char *out_buff, unsigned long value, unsigned char byte_count)
{
    unsigned int x;
    for(x=0; x < byte_count; x++)
	out_buff[x] = (value >> (byte_count -1 -x)*8) & 0xff;
    return 0;
}

int convertSBitsChar(unsigned char *out_buff, signed long value, unsigned char bit_count)
{
	unsigned int start=0;
	start = bit_count % 8;
    convertUBitsChar(out_buff, abs(value), bit_count);
    if(value < 0) {
		if(out_buff[0] & (1 << start))
	    	return -1; //num was too large.
		out_buff[0] |= (1 << start);
    } else if (out_buff[0] & (1 << start)) { //num was too large.
		return -1;
    }
    return 0;    
}

int convertUBitsChar(unsigned char *out_buff, unsigned long value, unsigned char bit_count)
{
    unsigned int start, start_byte,x, byte;
    start = bit_count % 8;
    x = start;
    if(start)
    	start_byte=1;
    else
    	start_byte=0;
    for(x=start, byte=start_byte; x < bit_count; x+=8, byte++)
    	out_buff[byte] = (value >> (bit_count -8 -x)) & 0xff;
    out_buff[0] = (value >> (bit_count -8 - start)) & 0xff;
	//out_buff[byte] = (value >> (byte_count -1 -x)*8) & 0xff;
    return 0;
}
