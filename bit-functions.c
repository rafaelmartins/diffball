#include <stdlib.h>
#include "bit-functions.h"
inline int bitsNeeded(long y)
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

inline int unsignedBytesNeeded(long y)
{
    unsigned int x;
    if (y == 0) {
	//printf("no bytesneeded\n");
	return 0;
    }
    x=bitsNeeded(y);
    x= (x/8) + (x % 8 ? 1 : 0);
    return x;
}
inline int signedBytesNeeded(signed long y)
{
    unsigned int x;
    x=bitsNeeded(abs(y)) + 1;
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
