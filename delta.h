#ifndef _DELTA_HEADER
#define _DELTA_HEADER 1
char *OneHalfPassCorrecting(unsigned char *ref, unsigned long ref_len,
    unsigned char *ver, unsigned long ver_len, unsigned int seed_len);

struct deltaCommandLoc {
    unsigned long offset;
    unsigned char len;
};

#endif
