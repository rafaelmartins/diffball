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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/evp.h>
#include "tar.h"
#include "string-misc.h"

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

/* I'll flat out state the previous var is a kludge to deal w/ gnu extensions.
   hopefully that'll be removed, or this will be integrated into read_fh_to... */
/*struct tar_entry *convert_str_tar_entry(char *block, struct tar_entry *previous)
{
    
    return t;
}*/

struct tar_entry **read_fh_to_tar_entry(int src_fh, unsigned long *total_count, unsigned char *md5sum)
{
    struct tar_entry **file, *entry;
    unsigned int md5len;
    unsigned char block[512], temp[3], hexmd5[EVP_MAX_MD_SIZE];
    //unsigned int count=0, offset=0;
    unsigned long offset=0, array_size=100000;
    unsigned long count =0;
    unsigned int read_bytes;
    unsigned int name_len, prefix_len;
    unsigned int extra_size;
    /* md5 stuff */
    EVP_MD_CTX mdctx;
    const EVP_MD *md;
    if((file = (struct tar_entry **)calloc(array_size,sizeof(struct tar_entry *)))==NULL){
	    perror("crud, couldn't allocate necesary memory.  What gives?\n");
	    exit(EXIT_FAILURE);
	}
    /*if(command_pipes("md5sum", "-", pipes)){
        perror("failed opening md5sum pipes, wtf?\n");
        exit(EXIT_FAILURE);
    }*/
    OpenSSL_add_all_digests();
    md = EVP_get_digestbyname("md5");
    if (!md) {
		perror("wrong digest name schmuck.  fix it.\n");
		exit(1);
    }
    EVP_DigestInit(&mdctx, md);

	extra_size=0;
    while((read_bytes=read(src_fh, block, 512))==512 /*&& strnlen(block)!=0*/) {
		EVP_DigestUpdate(&mdctx, block, 512);
		//printf("count(%lu)\n", count);
		if(strnlen(block, 512)==0)  {
			//printf("encountered zero length block, len(%u)\n");
			//printf("block was '%.512s'\n", block);
			break;
		}
		if((entry=(struct tar_entry *)malloc(sizeof(struct tar_entry)))==NULL){
			printf("shite, low on good ole mem.\n");
			abort();
		}
		if (! check_str_chksum((const char *)block)) {
		    printf("shite chksum didn't match on tar header\n");
		    abort();
		}
		if('L'==block[TAR_TYPEFLAG_LOC]) {
			extra_size = 1024;
			printf("handling longlink\n");
			entry->size = octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN);
			if((read_bytes=read(src_fh, block, 512))!=512) {
				printf("shite, unexpected eof\n");
				abort();
			}
			EVP_DigestUpdate(&mdctx, block, 512);
			name_len = strnlen(block, entry->size);
			if((entry->working_name = entry->fullname = 
				(unsigned char *)malloc(name_len))==NULL){
				printf("shite, lack o' the good ole mem.\n");
				abort();
			}
			memcpy(entry->fullname, block, entry->size);
			if((read_bytes=read(src_fh, block, 512))!=512){
				printf("shite, unexpected eof\n");
				abort();
			}
			EVP_DigestUpdate(&mdctx, block, 512);
			if(! check_str_chksum((const char *)block)) {
				printf("shite chksum didn't match on tar header\n");
				abort();
			}
			entry->size = octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN);
			entry->file_loc = offset;
			offset += 2;
		} else {
			name_len = strnlen(block + TAR_NAME_LOC, TAR_NAME_LEN) + 1;
			prefix_len = strnlen(block + TAR_PREFIX_LOC, TAR_PREFIX_LEN);
			prefix_len += (prefix_len==0 ? 0 : 1);
			if((entry->working_name = entry->fullname = 
				(unsigned char *)malloc(name_len + prefix_len))==NULL){
				printf("shite, lack o' the good ole mem.\n");
				abort();
			}
			if(prefix_len) {
				memcpy(entry->fullname, block + TAR_PREFIX_LOC, prefix_len -1);
				entry->fullname[prefix_len] = '/';
				memcpy(entry->fullname + prefix_len, block + TAR_NAME_LOC, name_len -1);
				entry->working_len = entry->fullname_len = prefix_len + name_len;
				entry->fullname[entry->fullname_len - 1] = '\0';
			} else {
				memcpy(entry->fullname, block + TAR_NAME_LOC, name_len -1);
				entry->fullname[name_len -1] = '\0';
				entry->working_len = entry->fullname_len = name_len;
			}
			//printf("final name was %s\n", entry->fullname);
			entry->file_loc = offset;
			entry->size = octal_str2long(block + TAR_SIZE_LOC, TAR_SIZE_LEN);
		}
		if (entry->size !=0) {
            int x= entry->size>>9;
            if (entry->size % 512)
                x++;
            //lseek(src_fh, (long)(x * 512), 1);
            offset += x + 1;
            while(x-- > 0){
                if(read(src_fh, &block, 512)==512){
				    EVP_DigestUpdate(&mdctx, block, 512);
                } else 
                    perror("Unexpected end of file encountered, exiting\n");
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
            if ((file = (struct tar_entry **)realloc(
            	file,(array_size+=50000)*sizeof(struct tar_entry *)))==NULL){
                printf("Eh?  Ran out of room for file array...\n");
                exit(EXIT_FAILURE);
            }
        }
		file[count++] = entry;
    }
    //printf("exiting function, count(%lu)\n", count);
    *total_count = count;
    if ((file=(struct tar_entry **)realloc(file,count*sizeof(struct tar_entry *)))==NULL){
        perror("Shit.\nNo explanation, just Shit w/ a capital S.\n");
        exit(EXIT_FAILURE);
    }
    
    if(read_bytes>0) { /*finish outputing the file for md5summing */
		EVP_DigestUpdate(&mdctx, block, 512);
        while((read_bytes = read(src_fh, &block, 512))>0) {
		    EVP_DigestUpdate(&mdctx, block, read_bytes);
		}
    }
    
    /* I have no doubt there is a better method */
    EVP_DigestFinal(&mdctx, hexmd5, &md5len);
    /* write a better method for this.  returned is aparently in hex. */
    for(count=0; count < md5len; count++){
		snprintf(temp, 3, "%02x", hexmd5[count]);
		memcpy(md5sum + count*2, temp, 2);
    }
    return file;
}
