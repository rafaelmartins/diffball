#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
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

struct tar_entry **read_fh_to_tar_entry(int src_fh, unsigned long *total_count, char *md5sum)
{
    struct tar_entry **file, *entry;
    //struct tar_llist source, target, *src_ptr, *trg_ptr;
    char block[512];
    //unsigned int count=0, offset=0;
    unsigned long offset=0, array_size=100000;
    unsigned long count =0;
    unsigned int read_bytes;
    int pipes[2];
    if((file = (struct tar_entry **)calloc(array_size,sizeof(struct tar_entry *)))==NULL){
	    perror("crud, couldn't allocate necesary memory.  What gives?\n");
	    exit(EXIT_FAILURE);
	}
    if(command_pipes("md5sum", "-", pipes)){
        perror("failed opening md5sum pipes, wtf?\n");
        exit(EXIT_FAILURE);
    }
    while((read_bytes=read(src_fh, block, 512))==512 && strnlen(block)!=0) {
	write(pipes[1], block, 512);
	if (! check_str_chksum((const char *)&block)) {
	    perror("shite chksum didn't match on tar header\n");
	    exit(EXIT_FAILURE);
	}
	/* check for gnu extensions.  posix extensions will need to be checked too */
	if (block[TAR_TYPEFLAG_LOC] == (char )'L') {
        /* LongLink.  hence the need for previous. as I said, it's a hack at best.*/
	/* this also needs testing/verification. */
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
	    /* ensure md5 is handled */
	    write(pipes[1], data, 512);
	    //printf("encountered longlink.  rewrote the previous entry to '%s'\n", file[count-1]->fullname);
	    continue;
	}
	if((entry=(struct tar_entry *)malloc(sizeof(struct tar_entry)))==NULL){
	    printf("Shite.  Couldn't allocate needed memory...\n");
	    exit(EXIT_FAILURE);
	}
/* I'm using strncpy purely so that things get null padded for sure.  memcpy elsewhere most likely */
	//block = block;
	//strncpy((char *)tmp8, (const char *)block[TAR_MODE_LOC], TAR_MODE_LEN);
	//tmp8[8]='\0'; /* overkill? */
	//entry->mode = strtol((char *)tmp8, NULL, 8);
	entry->mode = octal_str2long(&block[TAR_MODE_LOC], TAR_MODE_LEN);
	/*strncpy((char *)tmp8, (const char *)block, 8);          tmp8[8]='\0';
	entry->uid = strtol((char *)tmp8, NULL, 8);                 block+=8;*/
	entry->uid = octal_str2long(&block[TAR_UID_LOC], TAR_UID_LEN);
	entry->gid = octal_str2long(&block[TAR_GID_LOC], TAR_GID_LEN);
	entry->size = octal_str2long(&block[TAR_SIZE_LOC], TAR_SIZE_LEN);
	//entry->mtime = octal_str2long(&block[TAR_MTIME_LOC], TAR_MTIME_LEN);
	strncpy((char *)entry->mtime, &block[TAR_MTIME_LOC], TAR_MTIME_LEN);
	entry->chksum = octal_str2long(&block[TAR_CHKSUM_LOC], TAR_CHKSUM_LEN);
	entry->typeflag = (unsigned char)block[TAR_TYPEFLAG_LOC];
	
	unsigned int l;
	l = strnlen(&block[TAR_LINKNAME_LOC], TAR_LINKNAME_LEN);
	if((entry->linkname=(char *)malloc(l+1))==NULL){
	    perror("shite, couldn't alloc.\n");
	    exit(1);
	} else {
	    if (l > 0)
		strncpy((char *)entry->linkname, &block[TAR_LINKNAME_LOC],l);
	    entry->linkname[l]='\0';
	}
	strncpy((char *)entry->magic, &block[TAR_MAGIC_LOC], TAR_MAGIC_LEN);
	strncpy((char *)entry->version, &block[TAR_VERSION_LOC], TAR_VERSION_LEN);
	l = strnlen(&block[TAR_UNAME_LOC], TAR_UNAME_LEN);
	if((entry->uname=(char *)malloc(l+1))==NULL){
	    perror("shite, couldn't alloc.\n");
	    exit(1);
	} else {
	    if (l > 0)
		strncpy((char *)entry->uname, &block[TAR_UNAME_LOC],l);
	    entry->linkname[l]='\0';
	}
	l = strnlen(&block[TAR_GNAME_LOC], TAR_GNAME_LEN);
	if((entry->gname=(char *)malloc(l+1))==NULL){
	    perror("shite, couldn't alloc.\n");
	    exit(1);
	} else {
	    if (l > 0)
		strncpy((char *)entry->gname, &block[TAR_GNAME_LOC],l);
	    entry->linkname[l]='\0';
	}
	entry->devmajor = octal_str2long(&block[TAR_DEVMAJOR_LOC], TAR_DEVMAJOR_LEN);
	entry->devminor = octal_str2long(&block[TAR_DEVMINOR_LOC], TAR_DEVMINOR_LEN);
	if((entry->prefix_len=strnlen(&block[TAR_PREFIX_LOC], TAR_PREFIX_LEN))==0) { /* ergo prefix is nothing */
	    l=strnlen((char *)(&block[TAR_NAME_LOC]), TAR_NAME_LEN);
	    if((entry->fullname_ptr = entry->fullname = (char *)malloc(l+1))==NULL) {
		perror("shite, couldn't alloc memory\n");
		exit(1);
	    }
	    strncpy((char *)entry->fullname, (char *)(&block[TAR_NAME_LOC]), l);
	    entry->name=(char *)entry->fullname;
	    entry->fullname[l]='\0';
	} else {
	    l=strnlen((char *)(&block[TAR_NAME_LOC]), TAR_NAME_LEN);
	    if((entry->fullname_ptr = entry->fullname = (char *)malloc(l + entry->prefix_len + 2))==NULL) {
		perror("shite, couldn't alloc memory\n");
		exit(1);
	    }
	    strncpy((char *)entry->fullname_ptr, &block[TAR_PREFIX_LOC], entry->prefix_len);
	    entry->fullname_ptr[entry->prefix_len]= '/';
	    entry->name = (char *)(entry->fullname_ptr + entry->prefix_len +1);
	    strncpy((char *)entry->name, (char *)(&block[TAR_NAME_LOC]), l);
	    entry->fullname_ptr[l + entry->prefix_len + 2] = '\0';
	    entry->prefix_len++; // increment it to include the trailing slash
	}
        entry->entry_num = count;
        entry->file_loc = offset;
        /*if (count==1021 || count==1022 || count==1023){
            printf("handling annoyance %u\n", count);
            printf("entry ptr(%lu), name(%.100s)\n", entry, (char *)(entry->name));
        }*/
        if (entry->size !=0) {
            int x= entry->size>>9;
            if (entry->size % 512)
                x++;
            //lseek(src_fh, (long)(x * 512), 1);
            offset += x + 1;
            while(x-- > 0){
                if(read(src_fh, &block, 512)==512){
                    write(pipes[1], &block, 512);
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
            /*
            } else {
                printf("resized array to %u\n", array_size);
            */
            }
            //array_size *= 5;
        }
        file[count++] = entry;
        /* kludge for testing to capture a segfault*/
        
        /*printf("0 :%.100s\n1 :%u\n2 :%u\n3 :%u\n4 :%u\n5 :%.12s\n6 :%u\n7 :%c\n8 :%.100s\n9 :%.6s\n10:%.2s\n11:%.32s\n12:%.32s\n13:%u\n14:%u\n15:%100s\n16:%u\n",
        entry.name, entry.mode, entry.uid, entry.gid, entry.size, entry.mtime, entry.chksum, entry.typeflag,
        entry.linkname, entry.magic, entry.version, entry.uname, entry.gname, entry.devmajor, entry.devminor,
        entry.prefix, entry.file_loc);*/
    }
    *total_count = count;
    if ((file=(struct tar_entry **)realloc(file,count*sizeof(struct tar_entry *)))==NULL){
        perror("Shit.\nNo explanation, just Shit w/ a capital S.\n");
        exit(EXIT_FAILURE);
    }
    if(read_bytes>0) { /*finish outputing the file for md5summing */
        write(pipes[1], &block, read_bytes);
        while((read_bytes = read(src_fh, &block, 512))>0)
            write(pipes[1], &block, read_bytes);
    }
    close(pipes[1]);  /* close the write pipe to md5 */
    if ((count=read(pipes[0], &block, 32))!=32) {
        perror("thats weird, md5sum didn't return 32 char's... bug most likely in diffball.\n");
        exit(EXIT_FAILURE);
    }
    memcpy((char *)md5sum, (char *)&block, 32);
    return file;
}
