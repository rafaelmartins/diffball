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

struct DCStats {
    unsigned long copy_count;
    unsigned long add_count;
    unsigned long offset_max;
    unsigned long offset_min;
    unsigned long add_len_max;
    unsigned long add_len_min;
    unsigned long copy_len_max;
    unsigned long copy_len_min;
    signed long   neg_offset_max;
    signed long   neg_offset_min;
    signed long   pos_offset_max;
    signed long   pos_offset_min;
    unsigned long adjacent_offset_max;
    unsigned long adjacent_offset_min;
};

#endif
