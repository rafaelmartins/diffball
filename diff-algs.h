#ifndef _HEADER_DIFF_ALGS
#define _HEADER_DIFF_ALGS 1
#include "cfile.h"
#include "dcbuffer.h"

char *OneHalfPassCorrecting(unsigned int encoding_type,
    unsigned int offset_type,
    unsigned char *ref, unsigned long ref_len,
    unsigned char *ver, unsigned long ver_len, unsigned int seed_len, 
    /*int out_fh*/ struct cfile *out_fh);


#endif
