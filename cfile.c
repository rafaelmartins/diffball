#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "cfile.h"

signed int copen(struct cfile *cfile, int fh, 
	unsigned long fh_start, unsigned long fh_end, unsigned int compressor_type, 
	unsigned int access_flags)
{
    cfile->raw_fh = fh;
    cfile->trans_fh_pos = cfile->raw_fh_pos = fh_start;
    cfile->raw_fh_start = fh_start;
    cfile->raw_fh_end = fh_end;
    cfile->raw_fh_len = fh_end == 0 ? 0 : fh_end - fh_start;
    cfile->compressor_type = compressor_type;
    cfile->access_flags = access_flags;
    /* while raw_buff_size is currently redundant, at some point I'll likely allow variable buffer size*/
    /* not now though... */
    if((cfile->raw_buff=malloc(CFILE_RAW_BUFF_SIZE))==NULL) {
    	printf("shite, couldn't alloc needed mem for trans_buff\n");
    	exit(1);
    }
    cfile->raw_size = CFILE_RAW_BUFF_SIZE;
    cfile->raw_filled = cfile->raw_pos = cfile->raw_buff;
    lseek(cfile->raw_fh, cfile->raw_fh_start, SEEK_SET);
    switch(compressor_type) {
    case NO_COMPRESSOR:
	    break;
	}
    //crefresh(cfile);
    return 0;
}

signed int cclose(struct cfile *cfile)
{
	unsigned int bytes_left, x;
	//printf("closing cfile, access_flags(%u)\n", cfile->access_flags);
	if(cfile->access_flags & CFILE_WONLY) {
		bytes_left = cfile->raw_filled - cfile->raw_pos;
		switch(cfile->compressor_type)
		{
		case NO_COMPRESSOR:
			x = write(cfile->raw_fh, cfile->raw_pos, bytes_left);
			return x== bytes_left ? 0 : 1;
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
		if(cfile->raw_pos == cfile->raw_filled) {
		crefresh(cfile);
	    	/* note this check needs some work/better error returning.  surpris surprise... */
	    	if(cfile->raw_filled == cfile->raw_buff) {
				return bytes_read;
	    	}
		}
		switch(cfile->compressor_type)
		{
	    case NO_COMPRESSOR:
	    	uncompr_bytes = MIN(len, cfile->raw_filled - cfile->raw_pos);
	    	memcpy(out_buff + bytes_read, cfile->raw_pos, uncompr_bytes);
			cfile->raw_pos += uncompr_bytes;
	    	break;
		}
		bytes_read += uncompr_bytes;
    }
    return bytes_read;
}

inline void crefresh(struct cfile *cfile)
{
	unsigned int bread;
	//printf("crefresh called pos(%lu)\n", cfile->raw_fh_pos);
	cfile->raw_fh_pos += cfile->raw_filled - cfile->raw_buff;
	bread = read(cfile->raw_fh, cfile->raw_buff, cfile->raw_size);
	//cfile->raw_buff_filled=read(cfile->fh, cfile->raw_buff, cfile->raw_buff_size);
	/* note this check needs some work/better error returning.  surpris surprise... */
	/*if(cfile->raw_buff_filled == 0) {
		return bytes_read;
	}*/
	cfile->raw_filled = cfile->raw_buff + bread;
	cfile->raw_pos = cfile ->raw_buff;
}

unsigned long cwrite(struct cfile *cfile, unsigned char *in_buff, unsigned long len)
{
    unsigned long bytes_wrote = 0, tmp;
    unsigned int /*compr_bytes = 0,*/ uncompr_bytes=0;
    while(len > bytes_wrote) {
		if(cfile->raw_filled - cfile->raw_buff >= cfile->raw_size) {
	    	if((tmp = write(cfile->raw_fh, cfile->raw_buff, cfile->raw_size))
	    	   != cfile->raw_size) {
				/* need better error handling here, this WOULD leave cfile basically fscked */
				printf("error writing of cwrite, wrote tmp(%lu) of size(%lu)\n", tmp,
				cfile->raw_size);
				return bytes_wrote;
	    	}
	    	cfile->raw_fh_pos += cfile->raw_size;
	    	cfile->raw_filled = cfile->raw_pos = cfile->raw_buff;
	    	/* note this check needs some work/better error returning.  surprise surprise... */
		}
		switch(cfile->compressor_type)
		{
	    case NO_COMPRESSOR:
		    uncompr_bytes = MIN(len - uncompr_bytes, (cfile->raw_buff + cfile->raw_size) - 
		    	cfile->raw_filled);
		    memcpy(cfile->raw_filled, in_buff + bytes_wrote, uncompr_bytes);
		    cfile->raw_filled += uncompr_bytes;
		    break;
		}
		bytes_wrote += uncompr_bytes;
	}
    return bytes_wrote;
}

unsigned long cposition(struct cfile *cfile)
{
	unsigned long pos;
	switch(cfile->compressor_type) {
	case NO_COMPRESSOR:
		/*this breaks for write cseeks.  fix. */
		pos=cfile->raw_fh_pos + (cfile->raw_pos- cfile->raw_buff);
	}
	return pos;
}


/* this needs great work to handle cwrite.  In general, I don't trust this yet*/
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
			raw_offset = (unsigned long)(cfile->raw_fh_pos + (cfile->raw_pos -
			cfile->raw_buff) + offset);
		else
			/*not implemented yet*/
			raw_offset=cfile->raw_fh_start;
		/*printf("cseek info: raw_fh_pos(%lu), buff_pos(%u) calc(%lu), offset(%d)\n", cfile->raw_fh_pos,
			cfile->raw_pos - cfile->raw_buff, cfile->raw_fh_pos + 
			(cfile->raw_pos - cfile->raw_buff), offset);
		printf("cseek: offset_type(%u), current_pos(%lu), desired(%lu), calculated(%lu)\n", 
			offset_type, cfile->raw_fh_pos, offset, raw_offset);*/
		uncompr_offset = lseek(cfile->raw_fh, raw_offset, SEEK_SET);
		cfile->raw_fh_pos = uncompr_offset;
		cfile->raw_pos = cfile->raw_filled = cfile->raw_buff;
	}
	//printf("cseeked.  raw_fh_pos(%lu)\n", cfile->raw_fh_pos);
	crefresh(cfile);
	//printf("cseekd, called crefresh, raw_fh_pos(%lu)\n", cfile->raw_fh_pos);
	return uncompr_offset;
}
