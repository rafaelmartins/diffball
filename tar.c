#include <stdio.h>

int verify_str_chksum(const char *entry);
struct tar_entry *convert_str_tar_entry(char *block);

struct tar_entry {
    unsigned char       *name[100];
    unsigned int        mode;
    unsigned int        uid;
    unsigned int        gid;
    unsigned long       size;
    /*unsigned long     mtime;*/
    unsigned char       *mtime[12];
    unsigned long       chksum;
    unsigned char       typeflag;
    unsigned char       *linkname[100];
    unsigned char       *magic[6];
    unsigned char       *version[2];
    unsigned char       *uname[32];
    unsigned char       *gname[32];
    unsigned int        devmajor;
    unsigned int        devminor;
    unsigned char       *prefix[155];
    unsigned long	file_loc;
    unsigned int        entry_num;
/* I'm sure there is a better method, but I'm tired of screwing w/ ptrs. */
    unsigned char       *fullname[256];  /*concattenation of prefix and name, 1 extra for null */
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
    char tmp8[9];
    char tmp12[13];
    struct tar_entry *t;
    unsigned long chksum;
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
        exit(1);
    }
    /* I'm using strncpy purely so that things get null padded for sure.  memcpy elsewhere most likely */
    strncpy((char *)t->name, (const char *)block, 100);
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
    t->typeflag = *block;                                   block++;
    strncpy((char *)t->linkname, (const char *)block,100);   block+=100;
    strncpy((char *)t->magic, (const char *)block, 6);       block+=6;
    strncpy((char *)t->version, (const char *)block, 2);     block+=2;
    strncpy((char *)t->uname, (const char *)block, 32);      block+=32;
    strncpy((char *)t->gname, (const char *)block, 32);      block+=32;
    strncpy((char *)tmp8, (const char *)block, 8);          tmp8[8]='\0';
    t->devmajor = strtol((char *)tmp8, NULL, 8);            block+=8;
    strncpy((char *)tmp8, (const char *)block, 8);          tmp8[8]='\0';
    t->devminor = strtol((char *)tmp8, NULL, 8);            block+=8;
    strncpy((char *)t->prefix, (const char *)block, 155);
    strncpy((char *)t->fullname, (char *)t->prefix, 155);
    strncat((char *)t->fullname, (char *)t->name, 100);
    return t;
    //printf("matched %i\n", sscanf((const char *)block,"%8c",t.mode));
}
