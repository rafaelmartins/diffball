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

inline unsigned int unsignedBitsNeeded(unsigned long int y);
inline unsigned int signedBitsNeeded(signed long int y);
inline unsigned int unsignedBytesNeeded(unsigned long int y);
inline unsigned int signedBytesNeeded(signed long int y);

unsigned long readUBytesBE(const unsigned char *buff, unsigned int l);
unsigned long readUBytesLE(const unsigned char *buff, unsigned int l);
signed long readSBytesBE(const unsigned char *buff, unsigned int l);
//signed long readSBytesLE(const unsigned char *buff, unsigned int l);

unsigned int writeUBytesBE(unsigned char *buff, unsigned long value, 
    unsigned int l);
unsigned int writeUBytesLE(unsigned char *buff, unsigned long value,
    unsigned int l);
unsigned int writeSBytesBE(unsigned char *buff, signed long value,
    unsigned int l);
unsigned int writeSBytesLE(unsigned char *buff, signed long value,
    unsigned int l);

unsigned int writeSBitsBE(unsigned char *out_buff, signed long value,
    unsigned int bit_count);
unsigned int writeUBitsBE(unsigned char *out_buff, unsigned long value, 
    unsigned int bit_count);

#endif

