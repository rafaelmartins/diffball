#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <openssl/evp.h>
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

/* I'll flat out state the previous var is a kludge to deal w/ gnu extensions.
   hopefully that'll be removed, or this will be integrated into read_fh_to... */
/*struct tar_entry *convert_str_tar_entry(char *block, struct tar_entry *previous)
{
    
    return t;
}*/

struct tar_entry **read_fh_to_tar_entry(int src_fh, unsigned long *total_count, unsigned char *md5sum)
{
    struct tar_entry **file, *entry;
    //struct tar_llist source, target, *src_ptr, *trg_ptr;
    char block[512];
    //unsigned int count=0, offset=0;
    unsigned long offset=0, array_size=100000;
    unsigned long count =0;
    unsigned int read_bytes;
    int pipes[2];
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
    while((read_bytes=read(src_fh, block, 512))==512 && strnlen(block)!=0) {
	EVP_DigestUpdate(&mdctx, block, 512);
	if (! check_str_chksum((const char *)&block)) {
	    perror("shite chksum didn't match on tar header\n");
	    exit(EXIT_FAILURE);
	}
	/* check for gnu extensions.  posix extensions will need to be checked too */
	if (block[TAR_TYPEFLAG_LOC] == (char )'L') {
        /* LongLink.  hence the need for previous. as I said, it's a hack at best.*/
	/* this also needs testing/verification. */
	/* first run, seems fine.  haven't checked w/ prefix.  further, check offset code */
	    char *rep;
	    char data[512];
	    unsigned int name_len=octal_str2long(&block[TAR_SIZE_LOC], TAR_SIZE_LEN);
	    if(read(src_fh, data, 512) != 512) {
		perror("eh?  unexpected end of file when handling gnu extension LongLink\n");
		exit(EXIT_FAILURE);
	    }
	    if ((rep = (char *)malloc(
		file[count-1]->prefix_len + name_len + 1)) == NULL) {
		perror("shite, ran out of memory.\n");
		exit(EXIT_FAILURE);
	    }
	    strncpy((char *)rep, file[count-1]->fullname, file[count-1]->prefix_len);
	    strncpy((char *)(rep + file[count-1]->prefix_len), data, name_len);
	    file[count-1]->fullname_ptr = (file[count-1]->fullname_ptr - file[count-1]->fullname) + rep;
	    free(file[count-1]->fullname);
	    file[count-1]->fullname = rep;
	    file[count-1]->name = rep + file[count-1]->prefix_len + 1;
	    file[count-1]->fullname[file[count-1]->prefix_len + name_len] = '\0';
	    EVP_DigestUpdate(&mdctx, data, 512);
	    //update offset, 1 for the header, 1 for the data block.
	    offset +=2;
	    continue;
	}
	if((entry=(struct tar_entry *)malloc(sizeof(struct tar_entry)))==NULL){
	    printf("Shite.  Couldn't allocate needed memory...\n");
	    exit(EXIT_FAILURE);
	}
/* I'm using strncpy purely so that things get null padded for sure.  memcpy elsewhere most likely */
	entry->mode = octal_str2long(&block[TAR_MODE_LOC], TAR_MODE_LEN);
	entry->uid = octal_str2long(&block[TAR_UID_LOC], TAR_UID_LEN);
	entry->gid = octal_str2long(&block[TAR_GID_LOC], TAR_GID_LEN);
	entry->size = octal_str2long(&block[TAR_SIZE_LOC], TAR_SIZE_LEN);
	strncpy((char *)entry->mtime, &block[TAR_MTIME_LOC], TAR_MTIME_LEN);
	entry->chksum = octal_str2long(&block[TAR_CHKSUM_LOC], TAR_CHKSUM_LEN);
	entry->typeflag = (unsigned char)block[TAR_TYPEFLAG_LOC];
	
	//unsigned int l;
	entry->linkname_len = strnlen(&block[TAR_LINKNAME_LOC], TAR_LINKNAME_LEN);
	if((entry->linkname=(char *)malloc(entry->linkname_len + 1))==NULL){
	    perror("shite, couldn't alloc.\n");
	    exit(1);
	} else {
	    if (entry->linkname_len > 0)
		strncpy((char *)entry->linkname, &block[TAR_LINKNAME_LOC],
		    entry->linkname_len);
	    entry->linkname[entry->linkname_len]='\0';
	}
	strncpy((char *)entry->magic, &block[TAR_MAGIC_LOC], TAR_MAGIC_LEN);
	strncpy((char *)entry->version, &block[TAR_VERSION_LOC], TAR_VERSION_LEN);
	entry->uname_len = strnlen(&block[TAR_UNAME_LOC], TAR_UNAME_LEN);
	if((entry->uname=(char *)malloc(entry->uname_len + 1))==NULL){
	    perror("shite, couldn't alloc.\n");
	    exit(1);
	} else {
	    if (entry->uname_len > 0)
		strncpy((char *)entry->uname, &block[TAR_UNAME_LOC], entry->uname_len);
	    entry->uname[entry->uname_len]='\0';
	}
	
	entry->gname_len = strnlen(&block[TAR_GNAME_LOC], TAR_GNAME_LEN);
	if((entry->gname=(char *)malloc(entry->gname_len + 1))==NULL){
	    perror("shite, couldn't alloc.\n");
	    exit(1);
	} else {
	    if (entry->gname_len > 0)
		strncpy((char *)entry->gname, &block[TAR_GNAME_LOC],entry->gname_len);
	    entry->gname[entry->gname_len]='\0';
	}
	entry->devmajor = octal_str2long(&block[TAR_DEVMAJOR_LOC], TAR_DEVMAJOR_LEN);
	entry->devminor = octal_str2long(&block[TAR_DEVMINOR_LOC], TAR_DEVMINOR_LEN);
	if((entry->prefix_len=strnlen(&block[TAR_PREFIX_LOC], TAR_PREFIX_LEN))==0) { /* ergo prefix is nothing */
	    entry->name_len = strnlen((char *)(&block[TAR_NAME_LOC]), TAR_NAME_LEN);
	    if((entry->fullname_ptr = entry->fullname =
		    (char *)malloc(entry->name_len + 1))==NULL) {
		perror("shite, couldn't alloc memory\n");
		exit(1);
	    }
	    strncpy((char *)entry->fullname, (char *)(&block[TAR_NAME_LOC]),
		entry->name_len);
	    entry->name=(char *)entry->fullname;
	    entry->fullname[entry->name_len]='\0';
	} else {
	    entry->name_len=strnlen((char *)(&block[TAR_NAME_LOC]), TAR_NAME_LEN);
	    if((entry->fullname_ptr = entry->fullname =
		    (char *)malloc(entry->name_len + entry->prefix_len + 2))==NULL) {
		perror("shite, couldn't alloc memory\n");
		exit(1);
	    }
	    strncpy((char *)entry->fullname_ptr, &block[TAR_PREFIX_LOC], entry->prefix_len);
	    entry->fullname_ptr[entry->prefix_len]= '/';
	    entry->name = (char *)(entry->fullname_ptr + entry->prefix_len +1);
	    strncpy((char *)entry->name, (char *)(&block[TAR_NAME_LOC]), entry->name_len);
	    entry->fullname_ptr[entry->name_len + entry->prefix_len + 2] = '\0';
	    entry->prefix_len++; // increment it to include the trailing slash
	}
        entry->entry_num = count;
        entry->file_loc = offset;
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
        if(count==array_size) {
            /* out of room, resize */
            if ((file=(struct tar_entry **)realloc(file,(array_size+=50000)*sizeof(struct tar_entry *)))==NULL){
                perror("Eh?  Ran out of room for file array...\n");
                exit(EXIT_FAILURE);
            }
        }
        file[count++] = entry;
    }
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
    unsigned int md5len;
    unsigned char hexmd5[EVP_MAX_MD_SIZE];
    EVP_DigestFinal(&mdctx, hexmd5, &md5len);
    /* write a better method for this.  returned is aparently in hex. */
    unsigned char temp[3];
    for(count=0; count < md5len; count++){
	snprintf(temp, 3, "%02x", hexmd5[count]);
	memcpy(md5sum + count*2, temp, 2);
    }
    return file;
}
