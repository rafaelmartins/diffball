#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int verify_str_chksum(const char *entry);
struct tar_entry *convert_str_tar_entry(char *block);

struct tar_entry {
    unsigned char       *name;
    unsigned long int   mode;
    unsigned long int   uid;
    unsigned long int   gid;
    /* float, at least on my machine can represent 8**12, is this true for all archs? */
    /* fix this obviously. */
    unsigned long int   size;
    /*unsigned long     mtime;*/
    unsigned char       mtime[12];
    unsigned long       chksum;
    unsigned char       typeflag;
    unsigned char       *linkname;
    unsigned char       magic[6];
    unsigned char       version[2];
    unsigned char       *uname;
    unsigned char       *gname;
    unsigned long int   devmajor;
    unsigned long int   devminor;
    unsigned long int	file_loc;
    unsigned long int   entry_num;
    unsigned int        prefix_len;
    unsigned char       *fullname;  /*concattenation of prefix and name, 1 extra for null */
    unsigned char       *fullname_ptr;
    
};

int verify_str_chksum(const char *block)
{
    unsigned long chksum=0;
    unsigned long desired_chksum=0;
    unsigned char tmp8[9];
    unsigned int x;
    //printf("passed block='%.512s'\n", (char *)block);
    /* grab the chksum from the block. */
    strncpy((char *)&tmp8, (const char*)(block+148), 8);
    tmp8[8]='\0';
    desired_chksum=(unsigned long)strtol((const char *)tmp8, NULL, 8);
    //printf("desired chksum(%lu): ", desired_chksum);
    for(x=0; x< 148; x++) {
        //printf("%.5s", entry[x]);
        //printf("%c",(char)entry[x]);
        chksum += (unsigned char)block[x];
    }
    //chksum'ing treats the stored chksum as 8 blank chars.
    chksum+=256;
    //skip 12 bytes at 148, cause the chksum is there, and it's to be treated as 0's...
    for(x=156; x< 512; x++)
        chksum += (unsigned char)block[x];
    //fprintf(stderr, "computed('%lu')\n", chksum);
    return (desired_chksum != chksum );
}

struct tar_entry *convert_str_tar_entry(char *block/*[512]*/)
{
    unsigned int l=0;
    char tmp8[9];
    char tmp12[13];
    struct tar_entry *t;
    unsigned long chksum;
    unsigned int memuse=0;
    /* = (struct tar_entry *)malloc(sizeof(struct tar_entry)); */
    //t.name = *char[100];
    /* find out why the fuck it's bitching about god damned type conversions when the type is right.*/
    /*That Is Fucking Annoying.*/
    if (verify_str_chksum(block)!=0) {
        fprintf(stderr, "shite chksum didn't match on tar header\n");
        //fprintf(stderr, "correct chksum('%lu') != computed chksum('%lu')\n", t->chksum, chksum);
        //fprintf(stderr, "shite, chksum didn't match for block, name('%.100s')\n", t->name);
        exit(EXIT_FAILURE);
    }
    if((t=(struct tar_entry *)malloc(sizeof(struct tar_entry)))==NULL){
        printf("Shite.  Couldn't allocate needed memory...\n");
        exit(EXIT_FAILURE);
    }
   memuse=sizeof(struct tar_entry);
    /* I'm using strncpy purely so that things get null padded for sure.  memcpy elsewhere most likely */
    //strncpy((char *)t->name, (const char *)block, 100);
    block = block +100;
    strncpy((char *)tmp8, (const char *)block, 8);          tmp8[8]='\0'; /* overkill? */
    t->mode = strtol((char *)tmp8, NULL, 8);                block+=8;
    strncpy((char *)tmp8, (const char *)block, 8);          tmp8[8]='\0';
    t->uid = strtol((char *)tmp8, NULL, 8);                 block+=8;
    strncpy((char *)tmp8, (const char *)block, 8);          tmp8[8]='\0';
    t->gid = strtol((char *)tmp8, NULL,8);                  block+=8;
    strncpy((char *)tmp12, (const char *)block,12);         tmp12[12]='\0';
    t->size = (unsigned long)strtol((char *)tmp12, NULL,8);               block+=12;
    /*strncpy((char *)tmp12, (const char *)block, 12);        tmp12[12]='\0';
    t->mtime = strtol((char *)tmp12, NULL, 12);             block+=12;*/
    strncpy((char *)t->mtime, (const char *)block, 12);      block+=12;
    strncpy((char *)tmp8, (const char *)block, 8);          tmp12[8]='\0';
    t->chksum = (unsigned long)strtol((char *)tmp8, NULL,8);               block+=8;    
    t->typeflag = *block;                                    block++;
    l = strnlen(block, 100);
    if((t->linkname = (char *)calloc(sizeof(char), l +1))==NULL) {
        perror("not enough memory.\n");
        exit(EXIT_FAILURE);
    }
    memuse+=l+1;
    if(l != 0)
        strncpy((char *)t->linkname, (const char *)block,100);
    t->linkname[l]='\0';
    block+=100;
    strncpy((char *)t->magic, (const char *)block, 6);       block+=6;
    strncpy((char *)t->version, (const char *)block, 2);     block+=2;
    l=strnlen(block,32);
    if((t->uname = (char *)calloc(sizeof(char), l+1))==NULL) {
        perror("not enough memory: sec0\n");
        exit(EXIT_FAILURE);
    }
    memuse+=l+1;
    if(l != 0)
        strncpy((char *)t->uname, (const char *)block, 32);
    t->uname[l]='\0';
    block+=32;
    l=strnlen(block, 32);
    if((t->gname = (char *)calloc(sizeof(char), l+1))==NULL) {
        perror("not enough memory: sec1\n");
        exit(EXIT_FAILURE);
    }
    if(l != 0)
        strncpy((char *)t->gname, (const char *)block, 32);
    t->gname[l] = '\0';
    block+=32;
    strncpy((char *)tmp8, (const char *)block, 8);          tmp8[8]='\0';
    t->devmajor = strtol((char *)tmp8, NULL, 8);            block+=8;
    strncpy((char *)tmp8, (const char *)block, 8);          tmp8[8]='\0';
    t->devminor = strtol((char *)tmp8, NULL, 8);            block+=8;
    if ( (t->prefix_len = strnlen(block, 155)) == 0) { /* ergo prefix is nothing */
        if( (t->fullname_ptr = t->fullname = (char *)calloc(sizeof(char), strnlen(block-345, 100) + 1)) == NULL) {
            perror("not enough memory:sec2\n");
            exit(EXIT_FAILURE);
        }
	memuse+=(int)sizeof(char)*(strnlen((char*)(block-345), 100)+1) ;
        strncpy((char *)t->fullname, (char *)(block -345), 100);
        t->name=(char *)t->fullname;
    } else {
        if( (t->fullname_ptr = t->fullname=(char *)calloc(sizeof(char), t->prefix_len + strnlen(block-345, 100) + 2)) == NULL) {
            perror("not enough memory:sec3\n");
            exit(EXIT_FAILURE);
        }
	memuse+=sizeof(char)*(strnlen((char *)(block-345), 100) +t->prefix_len +3);
        strncpy((char *)t->fullname_ptr, (char *)block, t->prefix_len);
        t->fullname_ptr[t->prefix_len + 1]=(char)'/';
        t->name = (char *)(t->fullname_ptr + t->prefix_len + 2);
        strncpy((char *)t->name, (char *)(block -345), 100);
        t->fullname_ptr[strnlen((char *)t->fullname_ptr, 255) +1] = '\0';
    }
    printf("%5u:%s\n", memuse, t->fullname);
    return t;
}
