#ifndef _DELTA_HEADER
#define _DELTA_HEADER 1
#define DC_ADD 0
#define DC_COPY 1

char *OneHalfPassCorrecting(unsigned char *ref, unsigned long ref_len,
    unsigned char *ver, unsigned long ver_len, unsigned int seed_len, 
    int out_fh);

struct DCLoc {
    unsigned long offset;
    unsigned long len;
};

struct CommandBuffer {
    unsigned long count;
    unsigned long max_commands;
    unsigned char *cb_start, *cb_end, *cb_head, *cb_tail;
    unsigned char cb_tail_bit, cb_head_bit;
    struct DCLoc *lb_start, *lb_end, *lb_head, *lb_tail;
};

#endif
