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
#ifndef _HEADER_TAR
#define _HEADER_TAR 1
/* the data structs were largely taken/modified/copied from gnutar's header file.
   I had to go there for the stupid gnu extension... */
#define TAR_NAME_LOC		0
#define TAR_MODE_LOC		100
#define TAR_UID_LOC		108
#define TAR_GID_LOC		116
#define TAR_SIZE_LOC		124
#define TAR_MTIME_LOC		136
#define TAR_CHKSUM_LOC		148
#define TAR_TYPEFLAG_LOC	156
#define TAR_LINKNAME_LOC	157
#define TAR_MAGIC_LOC		257
#define TAR_VERSION_LOC		263
#define TAR_UNAME_LOC		265
#define TAR_GNAME_LOC		297
#define TAR_DEVMAJOR_LOC	329
#define TAR_DEVMINOR_LOC	337
#define TAR_PREFIX_LOC		345

#define TAR_NAME_LEN		100
#define TAR_MODE_LEN		8
#define TAR_UID_LEN		8
#define TAR_GID_LEN		8
#define TAR_SIZE_LEN		12
#define TAR_MTIME_LEN		12
#define TAR_CHKSUM_LEN		8
#define TAR_TYPEFLAG_LEN	1
#define TAR_LINKNAME_LEN	100
#define TAR_MAGIC_LEN		6
#define TAR_VERSION_LEN		2
#define TAR_UNAME_LEN		32
#define TAR_GNAME_LEN		32
#define TAR_DEVMAJOR_LEN	8
#define TAR_DEVMINOR_LEN	8
#define TAR_PREFIX_LEN		155


int check_str_chksum(const char *entry);
inline unsigned long octal_str2long(char *string, unsigned int length);
struct tar_entry **read_fh_to_tar_entry(int src_fh, unsigned long *total_count);

struct tar_entry {
    unsigned long	file_loc;
    unsigned long       entry_num;
    unsigned long	size;
//concattenation of prefix and name, +1 extra for null, +1 for slash if prefix is not null 
    unsigned char       *fullname;
    unsigned short int  fullname_len;
    unsigned char	*working_name;
    unsigned int	working_len;
//    unsigned char       *fullname_ptr;
//    unsigned short int	fullname_ptr_len;
};

/*
struct tar_entry {
    unsigned char       *name;
    unsigned short int  name_len;
    unsigned long int   mode;
    unsigned long int   uid;
    unsigned long int   gid;
    unsigned long       size;
    unsigned char       mtime[12];
    unsigned long       chksum;
    unsigned char       typeflag;
    unsigned char       *linkname;
    unsigned short int  linkname_len;
    unsigned char       magic[6];
    unsigned char       version[2];
    unsigned char       *uname;
    unsigned short int  uname_len;
    unsigned char       *gname;
    unsigned short int  gname_len;
    unsigned long int   devmajor;
    unsigned long int   devminor;
    unsigned char       prefix_len;
    unsigned long	file_loc;
    unsigned int        entry_num;
//concattenation of prefix and name, +1 extra for null, +1 for slash if prefix is not null 
    unsigned char       *fullname;
    unsigned short int  fullname_len;
    unsigned char       *fullname_ptr;
    unsigned short int	fullname_ptr_len;
};*/

#endif

