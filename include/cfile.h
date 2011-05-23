/*
  Copyright (C) 2003-2005 Brian Harring

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

extern unsigned int cfile_verbosity;

#include <bzlib.h>
#include <zlib.h>

#define CFILE_DEFAULT_BUFFER_SIZE 		(4096)
//#define CFILE_DEFAULT_BUFFER_SIZE		(BUFSIZ)
#define NO_COMPRESSOR					(0x0)
#define GZIP_COMPRESSOR					(0x1)
#define BZIP2_COMPRESSOR				(0x2)
#define AUTODETECT_COMPRESSOR			(0x4)

#define CFILE_RONLY						(0x1)
#define CFILE_WONLY						(0x2)
#define CFILE_NEW						(0x10)
#define CFILE_READABLE					(0x1)
#define CFILE_WRITEABLE					(0x2)
#define CFILE_WR						(CFILE_READABLE | CFILE_WRITEABLE)
#define CFILE_OPEN_FH					(0x8)
#define CFILE_SEEKABLE					(0x10)
#define CFILE_BUFFER_ALL				(0x20)
#define CFILE_MEM_ALIAS					(0x40)
#define CFILE_CHILD_CFH					(0x80)
#define CFILE_EOF						(0x100)
#define CFILE_DATA_SEEK_NEEDED			(0x200)
#define CFILE_FREE_AT_CLOSING			(0x400)
#define CFILE_FLAG_BACKWARD_SEEKS		(0x800)

#define BZIP2_DEFAULT_COMPRESS_LEVEL		9
#ifdef DEBUG_CFILE
#define BZIP2_VERBOSITY_LEVEL				4
#else
#define BZIP2_VERBOSITY_LEVEL				0
#endif
#define BZIP2_DEFAULT_WORK_LEVEL			30

/*lseek type stuff
SEEK_SET
		The offset is set to offset bytes.
SEEK_CUR
		The offset is set to its current location plus off-
		set bytes.
SEEK_END
		The offset is set to the size of the file plus off-
		set bytes.*/
#define CSEEK_ABS				0
#define CSEEK_CUR				1
#define CSEEK_END				2
#define CSEEK_FSTART			3

/* errors */
#define IO_ERROR                (-1)
#define EOF_ERROR               (-2)
#define MEM_ERROR               (-3)

#define CFILE_DEFAULT_MEM_ALIAS_W_REALLOC 0x40000

// this is thrown when the api allows something, but code doesn't yet.
// case in point, compressors bound to a copen_mem cfile.
// doing it this route, rather then changing the api down the line.
#define UNSUPPORTED_OPT			(-6)

typedef struct _cfile *cfile_ptr;
typedef int (*copen_io_func)(cfile_ptr);
typedef unsigned int (*cclose_io_func)(cfile_ptr);
typedef ssize_t (*cwrite_io_func)(cfile_ptr, void *src, size_t len);
typedef ssize_t (*cread_io_func)(cfile_ptr, void *out, size_t len);
typedef ssize_t (*crefill_io_func)(cfile_ptr);
typedef ssize_t (*cflush_io_func)(cfile_ptr);
typedef ssize_t (*cseek_io_func)(cfile_ptr, size_t offset);

typedef struct _cfile_io {
	copen_io_func       open;
	cclose_io_func      close;
	cwrite_io_func      write;
	cread_io_func       read;
	crefill_io_func     refill;
	cflush_io_func      flush;
	cseek_io_func       seek;
} cfile_io;

typedef struct {
	size_t offset;
	size_t pos;
	size_t end;
	size_t size;
	size_t write_start;
	size_t write_end;
	unsigned char *buff;
} cfile_window;

typedef struct _cfile **cfile_ptr_array;
typedef unsigned short 		CFH_ID;
typedef signed int			ECFH_ID;

typedef struct _cfile {
	CFH_ID				cfh_id;
	int					raw_fh;
	size_t		raw_fh_len;
	unsigned int		compressor_type;
	unsigned int		access_flags;
	unsigned long		state_flags;
	int					err;
	union {
		struct {
			unsigned int		last;
			unsigned int		handle_count;
		} parent;
		unsigned int *last_ptr;
	} lseek_info;

	size_t		data_fh_offset;
	size_t		data_total_len;
	cfile_window		data;

	size_t		raw_fh_offset;
	size_t		raw_total_len;
	cfile_window		raw;

	/* io backing */
	cfile_io 			*io;

	/* compression crap */
	bz_stream			*bzs;
	z_stream			*zs;
	lzma_stream			*xzs;
	gzFile				gz_handle;

} cfile;

#define CFH_IS_SEEKABLE(cfh)		(((cfh)->access_flags & CFILE_SEEKABLE) > 1)
#define FREE_CFH_AT_CLOSE(cfh)		((cfh)->state_flags |= CFILE_FREE_AT_CLOSING)
#define CFH_IS_CHILD(cfh)		((cfh)->state_flags & CFILE_CHILD_CFH)

#define LAST_LSEEKER(cfh) (CFH_IS_CHILD(cfh) ?								\
	*((cfh)->lseek_info.last_ptr) : (cfh)->lseek_info.parent.last)
//#define LAST_LSEEKER(cfh) (CFH_IS_CHILD(cfh) &&	*((cfh)->lseek_info.last_ptr) || (cfh)->lseek_info.parent.last)

#define IS_LAST_LSEEKER(cfh) ( (cfh)->cfh_id == LAST_LSEEKER((cfh)) || ((cfh)->state_flags & CFILE_MEM_ALIAS) )

int internal_copen(cfile *cfh, int fh, 
	size_t raw_fh_start, size_t raw_fh_end,
	size_t data_fh_start, size_t data_fh_end,
	unsigned int compressor_type, unsigned int access_flags);

int copen_mem(cfile *cfh, unsigned char *buff, size_t len, unsigned int compressor_type, unsigned int access_flags);
int copen(cfile *cfh, const char *filename, unsigned int compressor_type, unsigned int access_flags);

int copen_child_cfh(cfile *cfh, cfile *parent, size_t fh_start,
	size_t fh_end, unsigned int compressor_type, unsigned int
	access_flags);

cfile *copen_dup_cfh(cfile *cfh);
int copen_dup_fd(cfile *cfh, int fh, size_t fh_start, size_t fh_end, 
	unsigned int compressor_type, unsigned int access_flags);

inline signed int ensure_lseek_position(cfile *cfh);
inline void flag_lseek_needed(cfile *cfh);
inline void set_last_lseeker(cfile *cfh);
unsigned int cclose(cfile *cfh);
ssize_t cread(cfile *cfh, void *out_buff, size_t len);
ssize_t cwrite(cfile *cfh, void *in_buff, size_t len);
ssize_t crefill(cfile *cfh);
ssize_t cflush(cfile *cfh);
size_t ctell(cfile *cfh, unsigned int tell_type);
signed int raw_ensure_position(cfile *cfh);
ssize_t cseek(cfile *cfh, ssize_t offset, int offset_type);
ssize_t copy_cfile_block(cfile *out_cfh, cfile *in_cfh, size_t in_offset, size_t len);
ssize_t copy_add_block(cfile *out_cfh, cfile *src_cfh, size_t src_offset, size_t len, void *extra);
size_t cfile_len(cfile *cfh);
size_t cfile_start_offset(cfile *cfh);
cfile_window *expose_page(cfile *cfh);
cfile_window *next_page(cfile *cfh);
cfile_window *prev_page(cfile *cfh);
#endif
