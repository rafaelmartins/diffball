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
#ifndef _HEADER_BIT_FUNCTIONS
#define _HEADER_BIT_FUNCTIONS 1

#define BYTE_BIT_COUNT 8
#define BYTE_BYTE_COUNT 1
#define SHORT_BIT_COUNT 16
#define SHORT_BYTE_COUNT 2
#define INT_BIT_COUNT 24
#define INT_BYTE_COUNT 4
#define LONG_BIT_COUNT 64
#define LONG_BYTE_COUNT 8

inline int unsignedBitsNeeded(unsigned long int y);
inline int signedBitsNeeded(signed long int y);
inline int unsignedBytesNeeded(unsigned long int y);
inline int signedBytesNeeded(signed long int y);
unsigned long readUnsignedBytes(const unsigned char *buff, unsigned char l);
signed long readSignedBytes(const unsigned char *buff, unsigned char l);
int convertUBytesChar(unsigned char *out_buff, unsigned long value, unsigned char byte_count);
int convertSBytesChar(unsigned char *out_buff, signed long value, unsigned char byte_count);
int convertUBitsChar(unsigned char *out_buff, unsigned long value, unsigned int bit_count);
int convertSBitsChar(unsigned char *out_buff, signed long value, unsigned int bit_count);
/*int writeUBytes(int fh, unsigned long value, unsigned char byte_count);*/

#endif

