#ifndef _HEADER_BIT_FUNCTIONS
#define _HEADER_BIT_FUNCTIONS 1

inline int bitsNeeded(long y);
inline int unsignedBytesNeeded(long y);
inline int signedBytesNeeded(signed long y);
unsigned long readUnsignedBytes(const unsigned char *buff, unsigned char l);
signed long readSignedBytes(const unsigned char *buff, unsigned char l);

#endif

