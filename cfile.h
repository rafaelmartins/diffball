#ifndef _HEADER_CFILE
#define _HEADER_CFILE
#define CFILE_RAW_BUFF_SIZE 8 * 4096
#define NO_COMPRESSOR      0
#define GZIP_COMRPESSOR    1
#define BZIP2_COMPRESSOR   2

#define MIN(x,y) ((x) < (y) ? (x) : (y))

struct cfile {
    int fh;
    unsigned long fh_pos;
    /*unsigned long fh_size;*/
    unsigned int compressor_type;
    unsigned char raw_buff[CFILE_RAW_BUFF_SIZE];
    /*unsigned char *raw_buff_pos_ptr;*/
    unsigned int raw_buff_size;
    unsigned int raw_buff_filled;
    unsigned int raw_buff_pos;
    unsigned long raw_buff_fh_pos;
};

void initcfile(struct cfile *cfile, int fh, unsigned long fh_start,
    unsigned int compressor_type);
unsigned long cread(struct cfile *cfile, unsigned char *out_buff, unsigned long len);
unsigned long cwrite(struct cfile *cfile, unsigned char *in_buff, unsigned long len);

#endif
