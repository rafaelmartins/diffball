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
#define CFILE_RAW_BUFF_SIZE   (4096)
#define CFILE_TRANS_BUFF_SIZE (4096)
#define NO_COMPRESSOR      0
#define GZIP_COMRPESSOR    1
#define BZIP2_COMPRESSOR   2
#define CFILE_RONLY 1
#define CFILE_WONLY 2

#define CFILE_RAW_BUFF_FULL		0x1
#define CFILE_TRANS_BUFF_FULL		0x2
#define CFILE_LENGTH_KNOWN		0x80
#define CFILE_MEM_ALIAS			0x40
#define CFILE_BUFFER_ALL		0x10

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

#define MIN(x,y) ((x) < (y) ? (x) : (y))

typedef struct {
	//raw_fh stuff
	int raw_fh;
	unsigned long byte_len;
	//unsigned long raw_fh_len;
	unsigned int  compressor_type;
	unsigned int  access_flags;
	unsigned long raw_fh_start;
	unsigned long raw_fh_end;
	//raw buff.
	unsigned char *raw_buff;
	unsigned int  raw_size;
	unsigned char *raw_filled;
	unsigned char *raw_pos;
	unsigned long raw_fh_pos;
	//translated/uncompressed buff.
	unsigned char *trans_buff;
	unsigned int  trans_size;
	unsigned char *trans_filled;
	unsigned char *trans_pos;
	unsigned long trans_fh_pos;
	//misc state info
	unsigned long state_flags;

} cfile;

signed int copen(cfile *cfh, int fh, unsigned long fh_start,
   unsigned long fh_end, unsigned int compressor_type, unsigned int access_flags);
signed int cmemopen(cfile *cfh, unsigned char *buff, 
	unsigned long fh_start, unsigned long fh_end, unsigned int compressor_type);
signed int cclose(cfile *cfh);
unsigned long cread(cfile *cfh, unsigned char *out_buff, unsigned long len);
unsigned long cwrite(cfile *cfh, unsigned char *in_buff, unsigned long len);
inline void crefresh(cfile *cfh);
unsigned long ctell(cfile *cfh, unsigned int tell_type);
unsigned long cseek(cfile *cfh, signed long offset, int offset_type);
unsigned long copy_cfile_block(cfile *out_cfh, cfile *in_cfh, 
    unsigned long in_offset, unsigned long len);
#endif
