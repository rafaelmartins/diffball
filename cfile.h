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
#ifndef _HEADER_CFILE
#define _HEADER_CFILE
//#define CFILE_RAW_BUFF_SIZE   (4096)
//#define CFILE_TRANS_BUFF_SIZE (4096)
#define CFILE_DEFAULT_BUFFER_SIZE (BUFSIZ)
//#define CFILE_DEFAULT_BUFFER_SIZE (1)
#define NO_COMPRESSOR      0
#define GZIP_COMRPESSOR    1
#define BZIP2_COMPRESSOR   2
#define CFILE_RONLY 1
#define CFILE_WONLY 2

#define CFILE_SEEK_NEEDED		(0x4000)
#define CFILE_MEM_ALIAS			(0x20)
#define CFILE_BUFFER_ALL		(0x10)


/*lseek type stuff
SEEK_SET
	The offset is set to offset bytes.
SEEK_CUR
	The offset is set to its current location plus off-
	set bytes.
SEEK_END
	The offset is set to the size of the file plus off-
	set bytes.*/
#define CSEEK_ABS		0
#define CSEEK_CUR		1
#define CSEEK_END		2
#define CSEEK_FSTART	3

typedef struct {
/* don't really need start I guess.. */
//    unsigned long start;
    unsigned long offset;
    unsigned long pos;
    unsigned long end;
    unsigned long size;
    unsigned char *buff;
} cfile_window;

typedef struct {
    int			raw_fh;
    unsigned long	raw_fh_len;
    unsigned int	compressor_type;
    unsigned int	access_flags;
    unsigned long	state_flags;

    unsigned long	data_fh_offset;
    unsigned long	data_total_len;
    cfile_window	data;

    unsigned long	raw_fh_offset;
    unsigned long	raw_total_len;
    cfile_window	raw;
} cfile;

unsigned int  copen(cfile *cfh, int fh, unsigned long fh_start,
    unsigned long fh_end, unsigned int compressor_type, 
    unsigned int access_flags);
unsigned int  cmemopen(cfile *cfh, unsigned char *buff_start, 
    unsigned long buff_len, unsigned long fh_offset, 
    unsigned int access_flags);
unsigned int  cclose(cfile *cfh);
unsigned long cread(cfile *cfh, unsigned char *out_buff, unsigned long len);
unsigned long cwrite(cfile *cfh, unsigned char *in_buff, unsigned long len);
unsigned long crefill(cfile *cfh);
unsigned long cflush(cfile *cfh);
unsigned long ctell(cfile *cfh, unsigned int tell_type);
unsigned long cseek(cfile *cfh, signed long offset, int offset_type);
unsigned long copy_cfile_block(cfile *out_cfh, cfile *in_cfh, 
    unsigned long in_offset, unsigned long len);
unsigned long cfile_len(cfile *cfh);
unsigned long cfile_start_offset(cfile *cfh);
#endif
