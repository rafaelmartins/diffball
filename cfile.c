#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "cfile.h"

/*struct cfile {
    int fh;
    unsigned long fh_pos;
    //unsigned long fh_size;
    unsigned int compressor_type;
    unsigned char raw_buff[CFILE_RAW_BUFF_SIZE];
    unsigned int raw_buff_size;
    unsigned int raw_buff_filled;  
};*/
void initcfile(struct cfile *cfile, int fh, unsigned long fh_start, unsigned int compressor_type)
{
    cfile->fh = fh;
    /* while raw_buff_size is currently redundant, at some point I'll likely allow variable buffer size*/
    /* not now though... */
    cfile->raw_buff_size = CFILE_RAW_BUFF_SIZE;
    cfile->fh_pos= fh_start;
    cfile->raw_buff_filled=0;
    cfile->raw_buff_pos=0;
}

unsigned long cread(struct cfile *cfile, unsigned char *out_buff, unsigned long len)
{
    unsigned int  uncompr_bytes=0;
    unsigned long bytes_read=0;
    while(len != bytes_read) {
	if(cfile->raw_buff_pos == cfile->raw_buff_filled) {
	    cfile->raw_buff_filled=read(cfile->fh, cfile->raw_buff, cfile->raw_buff_size);
	    /* note this check needs some work/better error returning.  surpris surprise... */
	    if(cfile->raw_buff_filled == 0) {
		return bytes_read;
	    }
	    cfile->raw_buff_pos=0;
	    cfile->fh_pos += cfile->raw_buff_size;
	}
	switch(cfile->compressor_type)
	{
	    case NO_COMPRESSOR:
	    uncompr_bytes = MIN(len, cfile->raw_buff_size - cfile->raw_buff_pos);
	    memcpy(out_buff + bytes_read, cfile->raw_buff + cfile->raw_buff_pos, uncompr_bytes);
	    cfile->raw_buff_pos += uncompr_bytes;
	    break;
	}
	bytes_read += uncompr_bytes;
    }
    
}

unsigned long cwrite(struct cfile *cfile, unsigned char *in_buff, unsigned long len)
{
    unsigned long bytes_wrote = 0, tmp;
    unsigned int compr_bytes = 0, uncompr_bytes=0;
    while(len != bytes_wrote) {
	if(cfile->raw_buff_pos == cfile->raw_buff_size) {
	    if((tmp=write(cfile->fh, cfile->raw_buff, cfile->raw_buff_size))
	       != cfile->raw_buff_filled) {
		bytes_wrote+=tmp;
		/* need better error handling here, this WOULD leave cfile basically fscked */
		return bytes_wrote;
	    }
	    cfile->raw_buff_filled=0;
	    cfile->raw_buff_pos=0;
	    cfile->fh_pos += cfile->raw_buff_size;
	    /* note this check needs some work/better error returning.  surpris surprise... */
	}
	switch(cfile->compressor_type)
	{
	    case NO_COMPRESSOR:
	    uncompr_bytes = MIN(len, cfile->raw_buff_size - cfile->raw_buff_pos);
	    memcpy(cfile->raw_buff + cfile->raw_buff_pos, in_buff, uncompr_bytes);
	    //memcpy(out_buff + bytes_read, cfile->raw_buff + cfile->raw_buff_pos, uncompr_bytes);
	    cfile->raw_buff_pos += uncompr_bytes;
	    break;
	}
	bytes_wrote += uncompr_bytes;
    }
}
