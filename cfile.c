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
#include <assert.h>
#include <fcntl.h>
//#include <openssl/evp.h>
#include "cfile.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))

unsigned int
cmemopen(cfile *cfh, unsigned char *buff_start, unsigned long buff_len,
    unsigned long fh_offset, unsigned int access_flags)
{
    cfh->access_flags = access_flags;
    cfh->compressor_type = NO_COMPRESSOR;
    cfh->data.buff = buff_start;
    cfh->data.size = cfh->data_total_len = cfh->data.end = buff_len;
    cfh->data.offset= cfh->data.pos = 0;
    cfh->data_fh_offset = fh_offset;
    cfh->state_flags = CFILE_MEM_ALIAS;
    return 0;
}

unsigned int
copen(cfile *cfh, int fh, unsigned long fh_start, unsigned long fh_end, 
    unsigned int compressor_type, unsigned int access_flags)
{
    const EVP_MD *md;
    /* this will need adjustment for compressed files */
    cfh->raw_fh = fh;
    cfh->access_flags = (access_flags & ~CFILE_COMPUTE_MD5);
    cfh->state_flags = 0;
    /*if((cfh->data = (cfile_window *)malloc(sizeof(cfile_window)))==NULL)
	abort();
    if((cfh->raw = (cfile_window *)malloc(sizeof(cfile_window)))==NULL)
	abort();*/
    cfh->data_md5 = NULL;
    if(access_flags & CFILE_COMPUTE_MD5) {
	if((cfh->data_md5_ctx = (EVP_MD_CTX *)malloc(sizeof(EVP_MD_CTX)))==NULL)
	    abort();
	cfh->state_flags |= CFILE_COMPUTE_MD5;
	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname("md5");
	EVP_DigestInit(cfh->data_md5_ctx, md);
	cfh->data_md5_pos = 0;
    } else {
	cfh->data_md5_ctx = NULL;
	/* watch this, may need to change it when compression comes about */
	cfh->data_md5_pos = cfh->data_total_len;
    }
    switch( cfh->compressor_type = compressor_type ) {
    case NO_COMPRESSOR: 
	cfh->data_fh_offset = fh_start;
	cfh->data_total_len = fh_end - fh_start;
	if((access_flags & CFILE_BUFFER_ALL) != 0)
	    cfh->data.size = cfh->data_total_len;
	else 
	    cfh->data.size = CFILE_DEFAULT_BUFFER_SIZE;
	cfh->raw.size = 0;
 	break;
    }
    cfh->raw.pos = cfh->raw.offset  = cfh->raw.end = cfh->data.pos = 
	cfh->data.offset = cfh->data.end = 0;
    if(cfh->data.size != 0) {
	if((cfh->data.buff = (unsigned char *)malloc(cfh->data.size))==NULL)
	    abort();
    } else
	cfh->data.buff = NULL;
    if(cfh->raw.size != 0) {
	printf("cfh->raw.size=%lu\n", cfh->raw.size);
	if((cfh->raw.buff = (unsigned char *)malloc(cfh->raw.size))==NULL)
	    abort();
    } else
	cfh->raw.buff = NULL;
    cfh->state_flags |= CFILE_SEEK_NEEDED;
    return 0;
}

unsigned int
cclose(cfile *cfh)
{
    if(cfh->access_flags & CFILE_WONLY) {
	cflush(cfh);
    }
    if(cfh->data.size)
	free(cfh->data.buff);
    if(cfh->raw.size)
	free(cfh->raw.buff);
    if(cfh->state_flags & CFILE_COMPUTE_MD5) {
	free(cfh->data_md5_ctx);
	if(cfh->state_flags & CFILE_MD5_FINALIZED)
	    free(cfh->data_md5);
	cfh->data_md5_pos = 0;
    }
    cfh->raw.pos = cfh->raw.end = cfh->raw.size = cfh->raw.offset = 
	cfh->data.pos = cfh->data.end = cfh->data.size = cfh->data.offset = 
	cfh->raw_total_len = cfh->data_total_len = 0;
    return 0;
}

unsigned long
cread(cfile *cfh, unsigned char *buff, unsigned long len)
{
    unsigned long bytes_wrote=0;
    unsigned long x;
    if(cfh->state_flags & CFILE_SEEK_NEEDED)
	cseek(cfh, cfh->data.offset, CSEEK_FSTART);	
    while(bytes_wrote != len) {
	if(cfh->data.end==cfh->data.pos) {
	    if(0==crefill(cfh))
		return(bytes_wrote);
	}
	x = MIN(cfh->data.end - cfh->data.pos, len - bytes_wrote);
	/* possible to get stuck in a loop here, fix this */
	memcpy(buff + bytes_wrote, cfh->data.buff + cfh->data.pos, x);
	bytes_wrote += x;
	cfh->data.pos += x;
    }
    return bytes_wrote;
}

unsigned long
cwrite(cfile *cfh, unsigned char *buff, unsigned long len)
{
    unsigned long bytes_wrote=0, x;
    while(bytes_wrote < len) {
	if(cfh->data.size==cfh->data.pos) {
	    cflush(cfh);
	    if(cfh->data.pos!=0)
		abort();
	}
	x = MIN(cfh->data.size - cfh->data.pos, len - bytes_wrote);
	memcpy(cfh->data.buff + cfh->data.pos, buff + bytes_wrote, x);
	bytes_wrote += x;
	cfh->data.pos += x;
    }
    return bytes_wrote;
}

unsigned long
cseek(cfile *cfh, signed long offset, int offset_type)
{
    signed long data_offset;
    cfh->state_flags &= ~CFILE_SEEK_NEEDED;
    if(CSEEK_ABS==offset_type) 
	data_offset = abs(offset);
    else if (CSEEK_CUR==offset_type)
	data_offset = cfh->data_fh_offset + cfh->data.offset + cfh->data.pos +
	    offset;
    else if (CSEEK_END==offset_type)
	data_offset = cfh->data_fh_offset + cfh->data_total_len + offset;
    else if (CSEEK_FSTART==offset_type)
	data_offset = cfh->data_fh_offset + offset;
    else
	abort(); /*lovely error handling eh? */
    data_offset -= cfh->data_fh_offset;
    assert(data_offset >= 0);
/* not sure on this next assert, probably should be < rather then <= */
    assert(data_offset <= cfh->data_total_len);

    /* see if the desired location is w/in the data buff */
    if(data_offset >= cfh->data.offset &&
	data_offset <  cfh->data.offset + cfh->data.size && 
	cfh->data.end > data_offset - cfh->data.offset) {

	cfh->data.pos = data_offset - cfh->data.offset;
	return (CSEEK_ABS==offset_type ? data_offset + cfh->data_fh_offset: 
	    data_offset);
    }
    cfh->data.offset = data_offset;
    cfh->data.pos = cfh->data.end = 0;
    switch(cfh->compressor_type) {
    case NO_COMPRESSOR:
 	if(lseek(cfh->raw_fh, data_offset + cfh->data_fh_offset, SEEK_SET) !=
	    data_offset + cfh->data_fh_offset)
	    abort();
	break;
    }
    return (CSEEK_ABS==offset_type ? data_offset + cfh->data_fh_offset : 
	data_offset);
}

unsigned long
ctell(cfile *cfh, unsigned int tell_type)
{
    if(CSEEK_ABS==tell_type)
	return cfh->data_fh_offset + cfh->data.offset + cfh->data.pos;
    else if (CSEEK_FSTART==tell_type)
	return cfh->data.offset + cfh->data.pos;
    else if (CSEEK_END==tell_type)
	return cfh->data_total_len - 
	    (cfh->data.offset + cfh->data.pos);
    return 0;
}

unsigned long 
cflush(cfile *cfh)
{
    if(cfh->data.pos!=0) {
        switch(cfh->compressor_type) {
	case NO_COMPRESSOR:
	    if(cfh->data.pos != 
		write(cfh->raw_fh, cfh->data.buff, cfh->data.pos))
		abort();
	    cfh->data.offset += cfh->data.pos;
	    cfh->data.pos=0;
	    break;
	}
    }
    return 0;
}

unsigned long 
crefill(cfile *cfh)
{
    unsigned long x;
    switch(cfh->compressor_type) {
    case NO_COMPRESSOR:
	x = read(cfh->raw_fh, cfh->data.buff, cfh->data.size);
	cfh->data.offset += cfh->data.end;
	cfh->data.end = x;
	cfh->data.pos = 0;
	break;
    }
    if((cfh->state_flags & CFILE_COMPUTE_MD5) &&
	((cfh->state_flags & CFILE_MD5_FINALIZED)==0) && 
	(cfh->data.offset == cfh->data_md5_pos)) {
	EVP_DigestUpdate(cfh->data_md5_ctx, cfh->data.buff, cfh->data.end);
	cfh->data_md5_pos += cfh->data.end;
    }
    return cfh->data.end;
}

inline unsigned long
cfile_len(cfile *cfh)
{
    return cfh->data_total_len;
}

inline unsigned long
cfile_start_offset(cfile *cfh)
{
    return cfh->data_fh_offset;
}

/* while I realize this may not *necessarily* belong in cfile, 
   eh, it's going here.
   deal with it.  */
unsigned long 
copy_cfile_block(cfile *out_cfh, cfile *in_cfh, unsigned long in_offset, 
    unsigned long len) 
{
    unsigned char buff[CFILE_DEFAULT_BUFFER_SIZE];
    unsigned int lb;
    unsigned long bytes_wrote=0;;
    if(in_offset!=cseek(in_cfh, in_offset, CSEEK_FSTART))
	abort();
    while(len) {
	lb = MIN(CFILE_DEFAULT_BUFFER_SIZE, len);
	if( (lb!=cread(in_cfh, buff, lb)) ||
	    (lb!=cwrite(out_cfh, buff, lb)) )
	    abort;
	len -= lb;
	bytes_wrote+=lb;
    }
    return bytes_wrote;
}

unsigned int
cfile_finalize_md5(cfile *cfh)
{
    unsigned int md5len;
    /* check to see if someone is being a bad monkey... */
    assert(cfh->state_flags & CFILE_COMPUTE_MD5);
    /* better handling needed... */
    if((cfh->data_md5 = (unsigned char *)malloc(16))==NULL)
	return 1;
    if(ctell(cfh, CSEEK_FSTART)!=cfh->data_md5_pos) 
	cseek(cfh, cfh->data_md5_pos, CSEEK_FSTART);

    /* basically read in all data needed. 
	since commiting of md5 data is done by crefill, call it till tis empty
	*/
    while(cfh->data_md5_pos != cfh->data_total_len) 
	crefill(cfh);
    EVP_DigestFinal(cfh->data_md5_ctx, cfh->data_md5, &md5len);
    cfh->state_flags |= CFILE_MD5_FINALIZED;
    return 0;
}

cfile_window *
expose_page(cfile *cfh)
{
    if(cfh->data.end==0) 
	crefill(cfh);
    return &cfh->data;
}

cfile_window *
next_page(cfile *cfh)
{
    crefill(cfh);
    return &cfh->data;
}

cfile_window *
prev_page(cfile *cfh)
{
    /* possibly due an error check or something here */
    cseek(cfh, -cfh->data.size, CSEEK_CUR);
    crefill(cfh);
    return &cfh->data;
}
