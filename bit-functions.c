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
