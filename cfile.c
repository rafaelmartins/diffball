/*
  Copyright (C) 2003 Brian Harring

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, US 
*/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "cfile.h"

signed int 
cmemopen(cfile *cfh, unsigned char *buff, unsigned long fh_start, 
    unsigned long fh_end, unsigned int compressor_type)
{
    cfh->state_flags= CFILE_MEM_ALIAS;
    cfh->access_flags = CFILE_RONLY;
    switch(compressor_type) {
	case NO_COMPRESSOR:
	cfh->compressor_type = NO_COMPRESSOR;
	break;
    }
    cfh->raw_fh_start = fh_start;
    cfh->raw_fh_end = fh_end;
    cfh->byte_len = fh_end - fh_start;
    cfh->raw_pos = cfh->raw_buff = buff;
    cfh->trans_fh_pos = cfh->raw_fh_pos = fh_start;
    cfh->raw_filled = buff + fh_end - fh_start;
    cfh->raw_size = CFILE_RAW_BUFF_SIZE;
    //printf("mem alias, filled(%lu), pos(%lu)\n", cfh->raw_filled - cfh->raw_buff,
	//cfh->raw_pos - cfh->raw_buff);
}

signed int 
copen(cfile *cfh, int fh, unsigned long fh_start, unsigned long fh_end, 
unsigned int compressor_type, unsigned int access_flags)
{
    if(access_flags & CFILE_BUFFER_ALL) {
	unsigned char *ptr;
	lseek(fh, fh_start, SEEK_SET);
	if((ptr = (unsigned char *)malloc(fh_end - fh_start))==NULL){
	    printf("shite, failed buffer_add\n");
	    abort();
	} else if(fh_end - fh_start != read(fh, ptr, fh_end - fh_start)) {
	    printf("shite, failed reading buffer_all\n");
	    abort();
	}
        cmemopen(cfh, ptr, fh_start, fh_end, NO_COMPRESSOR);
	cfh->state_flags |= CFILE_BUFFER_ALL;
	printf("calling w/ buffer_all(%lu)\n",
	cfh->state_flags & CFILE_BUFFER_ALL);
	return 0;
    }
    cfh->access_flags = access_flags;
    cfh->state_flags = 0;
    switch(compressor_type) {
	case NO_COMPRESSOR:
	cfh->compressor_type = NO_COMPRESSOR;
	break;
    }
    cfh->raw_fh = fh;
    cfh->trans_fh_pos = cfh->raw_fh_pos = fh_start;
    cfh->raw_fh_start = fh_start;
    cfh->raw_fh_end = fh_end;
    if(fh_end > fh_start || fh_end!=0) {
	cfh->state_flags |= CFILE_LENGTH_KNOWN;
	switch(compressor_type) {
	case NO_COMPRESSOR:
	    cfh->byte_len = fh_end - fh_start;
	    break;
	}
    }
    //cfh->access_flags = access_flags;
    /* while raw_buff_size is currently redundant, at some point I'll likely allow variable buffer size*/
    /* not now though... */
    printf("cfile: copen init\n");
    if((cfh->raw_buff=malloc(CFILE_RAW_BUFF_SIZE))==NULL) {
    	printf("shite, couldn't alloc needed mem for trans_buff\n");
    	exit(1);
    }
    cfh->raw_size = CFILE_RAW_BUFF_SIZE;
    cfh->raw_filled = cfh->raw_pos = cfh->raw_buff;
    //cfh->state_flags =0;
    lseek(cfh->raw_fh, cfh->raw_fh_start, SEEK_SET);
    switch(compressor_type) {
    case NO_COMPRESSOR:
	break;
    }
    //crefresh(cfh);
    return 0;
}

signed int 
cclose(cfile *cfh)
{
    unsigned int bytes_left, x;
    //printf("closing cfile, access_flags(%u)\n", cfh->access_flags);
    if(cfh->access_flags & CFILE_WONLY) {
	bytes_left = cfh->raw_filled - cfh->raw_pos;
	switch(cfh->compressor_type)
	{
	case NO_COMPRESSOR:
	    x = write(cfh->raw_fh, cfh->raw_pos, bytes_left);
	    return x== bytes_left ? 0 : 1;
	    break;
	}
    }
    if((cfh->state_flags & CFILE_MEM_ALIAS)==0/* || 
	(cfh->state_flags & CFILE_BUFFER_ALL)*/) {
	printf("copen: freeing memory\n");
	free(cfh->raw_buff);
    }
    cfh->raw_buff = cfh->raw_pos = cfh->raw_filled = NULL;
    return 0;
}

unsigned long 
cread(cfile *cfh, unsigned char *out_buff, unsigned long len)
{
    unsigned int  uncompr_bytes=0;
    unsigned long bytes_read=0;
    if(cfh->state_flags & CFILE_MEM_ALIAS) {
    	uncompr_bytes = MIN(len, cfh->raw_filled - cfh->raw_pos);
    	memcpy(out_buff, cfh->raw_pos, uncompr_bytes);
   	    cfh->raw_pos += uncompr_bytes;
   	    //printf("mem cread, raw_pos(%lu), uncompr_bytes(%lu), final(%lu), filled(%lu)\n",
   		//cfh->raw_pos - cfh->raw_buff - uncompr_bytes, uncompr_bytes, 
   		//cfh->raw_pos - cfh->raw_buff,
   		//cfh->raw_filled - cfh->raw_buff);
   	return uncompr_bytes;
    }
    while(len != bytes_read) {
    	//printf("still looping\n");
    	if(len < bytes_read) {
	    printf("shite!\n");
	    exit(1);
    	}
	if(cfh->raw_pos == cfh->raw_filled) {
	    //printf("cread: refreshing file len(%lu), bytes_read(%lu)\n",
		//len, bytes_read);
	    //printf("crefreshing file for len(%lu), pos(%lu), filled(%lu)\n", len,
		//cfh->raw_pos - cfh->raw_buff, cfh->raw_filled - cfh->raw_buff);
	    crefresh(cfh);
	    //printf("result being pos(%lu), filled(%lu)\n",
		//cfh->raw_pos - cfh->raw_buff, cfh->raw_filled - cfh->raw_buff);
	    /* note this check needs some work/better error returning.  surpris surprise... */
	    if(cfh->raw_filled == cfh->raw_pos) {
	    	printf("cread: len(%lu), bytes_read(%lu)\n", len, bytes_read);
	    	printf("shit, raw_filled == raw_buff\n");
		    //abort();
		    return bytes_read;
	    }
	}
	switch(cfh->compressor_type)
	{
	case NO_COMPRESSOR:
	    //printf("opt1(%lu), calced(%lu)\n", len - bytes_read, 
		//cfh->raw_filled - cfh->raw_pos);
	    //printf("len(%lu), bytes_read(%lu), val(%lu), raw_f(%lu), raw_p(%lu), other(%lu)\n", 
	    	//len, bytes_read, len-bytes_read,  
	    	//cfh->raw_filled - cfh->raw_buff, 
	    	//cfh->raw_pos - cfh->raw_buff, cfh->raw_filled - cfh->raw_pos); 
	    uncompr_bytes = MIN(len - bytes_read, cfh->raw_filled - cfh->raw_pos);
	    memcpy(out_buff + bytes_read, cfh->raw_pos, uncompr_bytes);
		//printf("uncompr_bytes(%lu), raw_pos prior(%lu)\n", 
		//uncompr_bytes, cfh->raw_pos - cfh->raw_buff);
	    cfh->raw_pos += uncompr_bytes;
	    break;
	}
	//printf("cread: uncompr_bytes(%u), raw_pos(%lu)\n",
	    //uncompr_bytes, cfh->raw_pos - cfh->raw_buff); 
	//printf("bytes_read(%lu) + uncompr_bytes == bytes(read(%lu)\n", 
	    //bytes_read, uncompr_bytes, uncompr_bytes+ bytes_read);
	//printf("bytes_read(%lu), uncompr_bytes(%lu)\n", bytes_read, uncompr_bytes);
	bytes_read += uncompr_bytes;
	//printf("bytes_read actual(%lu)\n", bytes_read);
	//printf("cread: new bytes_read(%u)\n", bytes_read);
    }
    //printf("leaving cread\n");
    return bytes_read;
}

inline void 
crefresh(cfile *cfh)
{
    unsigned int bread;
    //printf("crefresh called pos(%lu)\n", cfh->raw_fh_pos);
    if (cfh->state_flags & CFILE_MEM_ALIAS) {
	//printf("crefresh called on mem alias, ignoring and returning\n");
	return;
    } else {
	//printf("crefresh called on fh(%u)\n", cfh->raw_fh);
    }
    cfh->raw_fh_pos += cfh->raw_filled - cfh->raw_buff;
    bread = read(cfh->raw_fh, cfh->raw_buff, cfh->raw_size);
    cfh->raw_filled = cfh->raw_buff + bread;
    cfh->raw_pos = cfh->raw_buff;
}

unsigned long 
cwrite(cfile *cfh, unsigned char *in_buff, unsigned long len)
{
    unsigned long bytes_wrote = 0, tmp, x;
    unsigned int /*compr_bytes = 0,*/ uncompr_bytes=0;
    //printf("    cwrite: asked to write(%lu)\n", len);
    while(len > bytes_wrote) {
    	if(cfh->state_flags & CFILE_RAW_BUFF_FULL) {
    	    //printf("    cwrite flushing buffer\n");
    	    x = cfh->raw_filled - cfh->raw_buff;
    	    if((tmp = write(cfh->raw_fh, cfh->raw_buff, x))!=x) {
    		//reaching this inner block==bad
    		printf("error writing of cwrite, wrote tmp(%lu) of size(%lu)\n", 
    		    tmp, x);
    		exit(1);
    	    }
    	    cfh->raw_fh_pos += x;
    	    cfh->raw_filled = cfh->raw_pos = cfh->raw_buff;
    	}
    	switch(cfh->compressor_type) {
    	case NO_COMPRESSOR:
    	    uncompr_bytes = MIN(len - bytes_wrote, 
    		(cfh->raw_buff + cfh->raw_size) - cfh->raw_filled);
    	    memcpy(cfh->raw_filled, in_buff + bytes_wrote, uncompr_bytes);
    	    if((cfh->raw_filled - cfh->raw_buff) + uncompr_bytes == 
		cfh->raw_size) {
    		cfh->state_flags |= CFILE_RAW_BUFF_FULL;
    	    }
    	    cfh->raw_filled += uncompr_bytes;
    	    break;
    	}
    	bytes_wrote += uncompr_bytes;
    }
    return bytes_wrote;
}

unsigned long 
ctell(cfile *cfh, unsigned int tell_type)
{
    unsigned long pos=0;
    switch(cfh->compressor_type) {
    case NO_COMPRESSOR:
	/*this breaks for write cseeks.  fix. */
	switch(tell_type) {
	case CSEEK_ABS:
	    pos=cfh->raw_fh_pos + (cfh->raw_pos - cfh->raw_buff);
	    break;
	case CSEEK_FSTART:
	    pos=cfh->raw_fh_pos + (cfh->raw_pos - cfh->raw_buff) -
		cfh->raw_fh_start;
	}
    }
    return pos;
}


/* this needs great work to handle cwrite.  In general, I don't trust this yet*/
unsigned long 
cseek(cfile *cfh, signed long offset, int offset_type)
{
    unsigned long raw_offset;
    unsigned long uncompr_offset=0;
    switch(cfh->compressor_type)
    {
    case NO_COMPRESSOR:
	if(offset_type==CSEEK_END) {
	    raw_offset = cfh->raw_fh_start + cfh->byte_len + offset;
	} else if(offset_type==CSEEK_ABS || offset_type==CSEEK_FSTART) {
	    raw_offset = (offset_type==CSEEK_FSTART ? cfh->raw_fh_start : 0)
		+ (unsigned long)offset;
	} else if (offset_type==CSEEK_CUR) {
	    raw_offset = (unsigned long)(cfh->raw_fh_pos + (cfh->raw_pos -
		cfh->raw_buff) + offset);
	} else {
	    /*not implemented yet*/
	    raw_offset=cfh->raw_fh_start;
	}

	/*printf("cseek info: raw_fh_pos(%lu), buff_pos(%u) calc(%lu), offset(%d)\n", cfh->raw_fh_pos,
	    cfh->raw_pos - cfh->raw_buff, cfh->raw_fh_pos + 
	    (cfh->raw_pos - cfh->raw_buff), offset);
	printf("cseek: offset_type(%u), current_pos(%lu), desired(%lu), calculated(%lu)\n", 
	    offset_type, cfh->raw_fh_pos, offset, raw_offset);*/

	if(cfh->state_flags & CFILE_MEM_ALIAS) {
	    /*printf("mem_aliased\noffset(%ld), raw_offset(%lu)\n", offset, raw_offset);
	    printf("raw_fh_start(%lu)\n", cfh->raw_fh_start);
	    printf("raw_pos(%lu)\n", cfh->raw_pos);*/
	    //cfh->raw_pos = cfh->raw_pos + raw_offset;
			
	    cfh->raw_pos = cfh->raw_buff + raw_offset - cfh->raw_fh_start;
	    if(offset_type==CSEEK_ABS)
		return cfh->raw_pos - cfh->raw_buff + 
		    cfh->raw_fh_start;
	    return cfh->raw_pos - cfh->raw_buff;
	}
	uncompr_offset = lseek(cfh->raw_fh, raw_offset, SEEK_SET);
	cfh->raw_fh_pos = uncompr_offset;
	if(offset_type!=CSEEK_ABS) 
	    uncompr_offset -= cfh->raw_fh_start;
	cfh->raw_pos = cfh->raw_filled = cfh->raw_buff;
    }
    //printf("cseeked.  raw_fh_pos(%lu)\n", cfh->raw_fh_pos);
    crefresh(cfh);
    //printf("cseekd, called crefresh, raw_fh_pos(%lu)\n", cfh->raw_fh_pos);
    return uncompr_offset;
}

/* while I realize this may not *necessarily* belong in cfile, 
   eh, it's going here.
   deal with it.  */
unsigned long 
copy_cfile_block(cfile *out_cfh, cfile *in_cfh, unsigned long in_offset, 
    unsigned long len) 
{
#define BUFF_SIZE 4096
    unsigned char buff[BUFF_SIZE];
    unsigned int lb;
    unsigned long bytes_wrote=0;;
    if(in_offset!=cseek(in_cfh, in_offset, CSEEK_FSTART))
	abort();
    while(len) {
	lb = MIN(BUFF_SIZE, len);
	if( (lb!=cread(in_cfh, buff, lb)) ||
	    (lb!=cwrite(out_cfh, buff, lb)) )
	    abort;
	len -= lb;
	bytes_wrote+=lb;
    }
    return bytes_wrote;
}

