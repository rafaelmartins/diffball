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
#include <stdio.h>
#include "string-misc.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "defs.h"
#include "tar.h"

int check_str_chksum(const char *block)
{
    /* the chksum is 8 bytes and lives at byte 148.
      for where the chksum exists in the string, you treat each byte as ' '*/
    unsigned long chksum=0;
    unsigned int x;
    for(x=0; x< 148; x++) {
        chksum += (unsigned char)block[x];
    }
    chksum+=' '* 8;
    for(x=156; x< 512; x++)
        chksum += (unsigned char)block[x];
    return (chksum==octal_str2long((char *)(block + TAR_CHKSUM_LOC), TAR_CHKSUM_LEN));
}

/* possibly this could be done different, what of endptr of strtol?
   Frankly I worry about strtol trying to go too far and causing a segfault. */
inline unsigned long octal_str2long(char *string, unsigned int length)
{
    char *ptr = (char *)malloc(length +1);
    unsigned long x;
    strncpy((char *)ptr, (const char *)string, length);
    ptr[length]='\0'; /* overkill? */
    x = strtol((char *)ptr, NULL, 8);
    free(ptr);
    return(x);
}

int
read_fh_to_tar_entry(cfile *src_fh, tar_entry ***tar_entries, 
    unsigned long *total_count) 
{
    tar_entry *entry, **file;
    unsigned char block[512];
    unsigned long offset=0, array_size=100000;
    unsigned long count =0;
    unsigned int read_bytes;
    unsigned int name_len, prefix_len;
    unsigned int extra_size;
    if((file = (tar_entry **)calloc(array_size,sizeof(tar_entry *)))==NULL){
	return MEM_ERROR;
    }
    extra_size=0;
    while((read_bytes=cread(src_fh, block, 512))==512) {
	if(strnlen(block, 512)==0)  {
	    break;
	}
	if((entry=(tar_entry *)malloc(sizeof(tar_entry)))==NULL){
	    return MEM_ERROR;
	}
	if (! check_str_chksum((const char *)block)) {
	    return MEM_ERROR;
	}
	if('L'==block[TAR_TYPEFLAG_LOC]) {
	    extra_size = 1024;
	    v2printf("handling longlink\n");
	    name_len = octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN) -1;
	    if((read_bytes=cread(src_fh, block, 512))!=512) {
		return EOF_ERROR;
	    }
	    if((entry->fullname = 
		(unsigned char *)malloc(name_len))==NULL){
		return EOF_ERROR;
	    }
	    entry->working_name = entry->fullname;
	    memcpy(entry->fullname, block, name_len);
	    if((read_bytes=cread(src_fh, block, 512))!=512){
		return EOF_ERROR;
	    }
	    if(! check_str_chksum((const char *)block)) {
		return PATCH_CORRUPT_ERROR;
	    }
	    entry->working_len = name_len;
	    entry->size = octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN);
	    entry->file_loc = offset;
	    offset += 2;
	} else {
	    name_len = strnlen(block + TAR_NAME_LOC, TAR_NAME_LEN) + 1;
	    prefix_len = strnlen(block + TAR_PREFIX_LOC, TAR_PREFIX_LEN);
	    prefix_len += (prefix_len==0 ? 0 : 1);
	    if((entry->working_name = entry->fullname = 
		(unsigned char *)malloc(name_len + prefix_len))==NULL){
		return MEM_ERROR;
	    }
	    if(prefix_len) {
		memcpy(entry->fullname, block + TAR_PREFIX_LOC, prefix_len -1);
		entry->fullname[prefix_len] = '/';
		memcpy(entry->fullname + prefix_len, block + TAR_NAME_LOC, 
		    name_len -1);
		entry->working_len = entry->fullname_len = prefix_len + 
		    name_len;
		entry->fullname[entry->fullname_len - 1] = '\0';
	    } else {
		memcpy(entry->fullname, block + TAR_NAME_LOC, name_len -1);
		entry->fullname[name_len -1] = '\0';
		entry->working_len = entry->fullname_len = name_len;
	    }
	    entry->file_loc = offset;
	    entry->size = octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN);
	}
	if (entry->size !=0) {
            int x= entry->size>>9;
            if (entry->size % 512)
                x++;
            offset += x + 1;
            while(x-- > 0){
// wtf...
                if(cread(src_fh, block, 512)!=512){
		    return EOF_ERROR;
		}
            }    
        } else {
            offset++;
        }
        if(extra_size) {
            entry->size += extra_size;
            extra_size=0;
        }
        if(count==array_size) {
            /* out of room, resize */
            if ((file = (tar_entry **)realloc(
            	file,(array_size+=50000)*sizeof(tar_entry *)))==NULL){
		return MEM_ERROR;
            }
        }
	file[count++] = entry;
    }
    *total_count = count;
    if ((file=(tar_entry **)realloc(file,count*sizeof(tar_entry *)))==NULL){
	return MEM_ERROR;
    }
    *tar_entries = file;
    return 0;
}
