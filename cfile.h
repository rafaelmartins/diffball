#ifndef _HEADER_CFILE
#define _HEADER_CFILE
#define CFILE_RAW_BUFF_SIZE 32768 
 //8 * 4096
#define NO_COMPRESSOR      0
#define GZIP_COMRPESSOR    1
#define BZIP2_COMPRESSOR   2
#define CFILE_RONLY 1
#define CFILE_WONLY 2

/*lseek type stuff
SEEK_SET
	The offset is set to offset bytes.
SEEK_CUR
	The offset is set to its current location plus off-
	set bytes.
SEEK_END
	The offset is set to the size of the file plus off-
	set bytes.*/
#define CSEEK_SET 0
#define CSEEK_CUR 1
#define CSEEK_END 2

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
    unsigned int access_flags;
};

signed int copen(struct cfile *cfile, int fh, unsigned long fh_start,
    unsigned int compressor_type, unsigned int access_flags);
signed int cclose(struct cfile *cfile);
unsigned long cread(struct cfile *cfile, unsigned char *out_buff, unsigned long len);
unsigned long cwrite(struct cfile *cfile, unsigned char *in_buff, unsigned long len);
inline void crefresh(struct cfile *cfile);

#endif
