#include <unistd.h>
#include <string.h>
#include "cfile.h"

signed int copen(struct cfile *cfile, int fh, unsigned long fh_start, unsigned int compressor_type, 
	unsigned int access_flags)
{
    cfile->fh = fh;
    /* while raw_buff_size is currently redundant, at some point I'll likely allow variable buffer size*/
    /* not now though... */
    cfile->raw_buff_size = CFILE_RAW_BUFF_SIZE;
    cfile->raw_buff_fh_pos = cfile->fh_pos= fh_start;
    cfile->raw_buff_filled=0;
    cfile->raw_buff_pos=0;
    lseek(cfile->fh, cfile->fh_pos, SEEK_SET);
    cfile->access_flags = access_flags;
    //crefresh(cfile);
    return 0;
}

signed int cclose(struct cfile *cfile)
{
	unsigned int x;
	//printf("closing cfile, access_flags(%u)\n", cfile->access_flags);
	if(cfile->access_flags & CFILE_WONLY) {
		switch(cfile->compressor_type)
		{
		case NO_COMPRESSOR:
			x = write(cfile->fh, cfile->raw_buff, cfile->raw_buff_pos);
			return x== cfile->raw_buff_pos ? 0 : 1;
			break;
		}
	}
	return 0;
}

unsigned long cread(struct cfile *cfile, unsigned char *out_buff, unsigned long len)
{
    unsigned int  uncompr_bytes=0;
    unsigned long bytes_read=0;
    while(len != bytes_read) {
		if(cfile->raw_buff_pos == cfile->raw_buff_filled) {
		crefresh(cfile);
		    //printf("    filling cfile buffer, raw_buff_pos(%lu), filled(%lu)\n", cfile->raw_buff_pos,
		    //	cfile->raw_buff_filled);
		    //cfile->raw_buff_filled=read(cfile->fh, cfile->raw_buff, cfile->raw_buff_size);
	    	/* note this check needs some work/better error returning.  surpris surprise... */
	    	if(cfile->raw_buff_filled == 0) {
				return bytes_read;
	    	}
	    	//cfile->raw_buff_pos=0;
	    	//cfile->raw_buff_fh_pos += cfile->raw_buff_size;
	    	//cfile->fh_pos += cfile->raw_buff_size;
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
    cfile->fh_pos += bytes_read;
    return bytes_read;
}

inline void crefresh(struct cfile *cfile)
{
	cfile->raw_buff_filled=read(cfile->fh, cfile->raw_buff, cfile->raw_buff_size);
	/* note this check needs some work/better error returning.  surpris surprise... */
	/*if(cfile->raw_buff_filled == 0) {
		return bytes_read;
	}*/
	cfile->raw_buff_pos=0;
	cfile->raw_buff_fh_pos += cfile->raw_buff_size;
}

unsigned long cwrite(struct cfile *cfile, unsigned char *in_buff, unsigned long len)
{
    unsigned long bytes_wrote = 0, tmp;
    unsigned int /*compr_bytes = 0,*/ uncompr_bytes=0;
    while(len != bytes_wrote) {
		if(cfile->raw_buff_pos == cfile->raw_buff_size) {
	    	//printf("flushing write buffer\n");
	    	if((tmp=write(cfile->fh, cfile->raw_buff, cfile->raw_buff_size))
	    	   != cfile->raw_buff_filled) {
	    	   bytes_wrote+=tmp;
	    	   //printf("danger will robinson, danger.  killed early, wrote(%lu) of wrote(%lu)\n",
	    		//bytes_wrote, len);
				/* need better error handling here, this WOULD leave cfile basically fscked */
				return bytes_wrote;
	    	}
	    	cfile->raw_buff_pos=0;
	    	cfile->raw_buff_fh_pos += cfile->raw_buff_size;
	    	/* note this check needs some work/better error returning.  surprise surprise... */
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
    cfile->fh_pos += bytes_wrote;
    return bytes_wrote;
}

unsigned long cseek(struct cfile *cfile, signed long offset, int offset_type)
{
	unsigned long raw_offset;
	unsigned long uncompr_offset;
	switch(cfile->compressor_type)
	{
	case NO_COMPRESSOR:
		if(offset_type==CSEEK_SET)
			raw_offset = (unsigned long)offset;
		else if (offset_type==CSEEK_CUR)
			raw_offset = (unsigned long)((cfile->raw_buff_size -
			cfile->raw_buff_fh_pos) + cfile->raw_buff_pos + offset);
		else
			/*not implemented yet*/
			raw_offset=0;
		cfile->raw_buff_fh_pos = uncompr_offset = lseek(cfile->fh, raw_offset, SEEK_SET);
	}
	crefresh(cfile);
	return uncompr_offset;
}
