#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "delta.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))


//offset = fh_pos + readSignedBytes(cpy_buff, ctmp);
//len = readUnsignedBytes(cpy_buff+ctmp, clen);


unsigned long readUnsignedBytes(const unsigned char *buff, unsigned char l)
{
    unsigned char *p;
    unsigned long num=0;
    for(p = (unsigned char*)buff; p - (unsigned char*)buff < l; p++) {
	num = (num << 8) | *p;
    }
    return (unsigned long)num;
}

signed long readSignedBytes(const unsigned char *buff, unsigned char l)
{
    unsigned long num;
    unsigned char *p;
    num = *buff & 0x7f;  //strpi the leading bit.
    for(p = (unsigned char *)buff + 1; p - buff < l; p++) {
	num = (num << 8) + *p;
    }
    return (signed long)(num * (*buff & 0x80 ? -1 : 1));
}

int main(int argc, char **argv)
{
    unsigned long int x;
    struct stat src_stat, delta_stat;
    int src_fh, delta_fh, out_fh;
    signed long offset;
    unsigned long len;
    unsigned long fh_pos, delta_pos=0;
    unsigned int buff_filled;
    unsigned int clen, ctmp;
    unsigned char buffer[1024], cpy_buff[12];
    unsigned char commands[512], *cptr, ccom;
    if(argc <4){
	printf("pardon, but...\nI need at least 3 args- (reference file), patch-file, target file\n");
	exit(EXIT_FAILURE);
    }
    if(stat(argv[1], &src_stat)) {
	perror("what the hell, stat failed.  wtf?\n");
	exit(EXIT_FAILURE);
    }
    printf("src_fh size=%lu\n", src_stat.st_size);
    if ((src_fh = open(argv[1], O_RDONLY,0)) == -1) {
	printf("Couldn't open %s, does it exist?\n", argv[1]);
	exit(EXIT_FAILURE);
    }
    if((delta_fh = open(argv[2], O_RDONLY,0))==-1) {
	printf("Couldn't open patch file.\n");
	exit(EXIT_FAILURE);
    }
    if(stat(argv[2], &delta_stat)) {
	perror("what the hell, stat failed.  wtf?\n");
	exit(EXIT_FAILURE);
    }
    printf("delta_fh size=%lu\n", delta_stat.st_size);
    if((out_fh = open(argv[3], O_RDWR | O_TRUNC | O_CREAT,0))==-1) {
	printf("Couldn't create\truncate output file.\n");
	exit(EXIT_FAILURE);
    }
    if((buff_filled=read(delta_fh, commands, 512))==0){
	printf("ahem.  the delta file is empty?\n");
	exit(EXIT_FAILURE);
    }
    fh_pos=0;
    cptr=commands;
    //buff_filled=512;
    while(*cptr != 0) {
	if(*cptr > 0 && *cptr <= 248) {
	    //add command
	    ccom = *cptr;
	    cptr++;
	    printf("add  command delta_pos(%lu), fh_pos(%lu), len(%lu)\n", delta_pos, fh_pos, ccom);
	    clen = MIN(buff_filled - (cptr - commands), ccom);
	    //printf("len(%lu), clen(%lu)\n", *cptr, clen);
	    if(write(out_fh, cptr, clen)!= clen){
		printf("eh?  Tried writing(%u) bytes, but failed\n", ccom);
		exit(1);
	    }
	    fh_pos += ccom;
	    if(ccom != clen){
		clen=ccom - clen;
		if((buff_filled=read(delta_fh, commands, 512))==0){
		    printf("ahem.  eof encountered earlier then expected...\n");
		    exit(EXIT_FAILURE);
		}
		if(write(out_fh, commands, clen)!= clen){
		    printf("eh?  Tried writing(%u) bytes, but failed\n", *cptr);
		    exit(1);
		}
		cptr= commands + clen;
	    } else {
		cptr+=ccom;
	    }
	    delta_pos +=ccom + 1;	
	} else if(*cptr >= 249 ) {
	    //copy command
	    ccom=*cptr;
	    if( ccom  ==  249)
		clen=3;
	    else if(ccom==250)
		clen=4;
	    else if(ccom==251)
		clen=6;
	    else if(ccom==252)
		clen=5;
	    else if(ccom==253)
		clen=6;
	    else if(ccom==254)
		clen=8;
	    else
		clen=12;
	    ctmp=MIN(buff_filled - (cptr + 1 -commands), clen);
	    memcpy(cpy_buff, cptr+1, ctmp);
	    //printf("buffer stat, ccom(%u), ctmp(%lu), clen(%u), cptr(%lu)\n", ccom, ctmp, clen, commands);
	    if(ctmp!=clen){
		if((buff_filled=read(delta_fh, commands, 512))==0){
		    printf("ahem.  eof encountered earlier then expected...\n");
		    exit(EXIT_FAILURE);
		}
		memcpy(cpy_buff + ctmp, commands, clen - ctmp);
		cptr = commands + clen - ctmp;
	    } else {
		cptr += clen + 1;
	    }
	    // hokay, buffer shite is done w/.  now do actual copy command handling. 
	    if(ccom >=249 && ccom <=251)
		ctmp=2;
	    else if(ccom >=252 && ccom <=254)
		ctmp=4;
	    else 
		ctmp=8;
	    offset = readSignedBytes(cpy_buff, ctmp);
	    
	    if(ccom==249 || ccom==252)
		clen=1;
	    else if(ccom==250 || ccom==253)
		clen=2;
	    else
		clen=4;
	    len = readUnsignedBytes(cpy_buff+ctmp, clen);
	    printf("copy command delta_pos(%lu), fh_pos(%lu), type(%u), offset(%d), ref_pos(%lu) len(%lu)\n",
		delta_pos, fh_pos, ccom, offset, fh_pos + offset, len);
	    delta_pos += clen + ctmp + 1;
	    if(lseek(src_fh, fh_pos + offset, SEEK_SET)!= fh_pos + offset) {
		printf("well that's weird, couldn't lseek.\n");
		exit(EXIT_FAILURE);
	    }
	    fh_pos +=len;
	    while(len) {
		clen=(read(src_fh, buffer, MIN(1024, len)));
		if(clen != 1024 && clen != len) {
		    printf("hmm, error reading src_fh.\n");
		    printf("clen(%lu), len(%lu)\n", clen, len);
		    exit(EXIT_FAILURE);
		}
		if(write(out_fh, buffer, clen) != clen){
		    printf("hmm, error writing the versionned file.\n");
		    exit(EXIT_FAILURE);
		}
		len -= clen;
	    }
	}
	if(cptr == commands + buff_filled) {
	    printf("refreshing buffer: cptr(%u)==buff_filled(%u)\n", cptr - commands, buff_filled);
	    if((buff_filled=read(delta_fh, commands, 512))==0){
		printf("ahem.  the delta file is empty?\n");
		exit(EXIT_FAILURE);
	    }
	    cptr=commands;
	    //continue;
	}
	/*if(cptr == commands + buff_filled) {
	    printf("refreshing buffer: cptr(%u)==buff_filled(%u)\n", cptr - commands, buff_filled);
	    if((buff_filled=read(delta_fh, commands, 512))==0){
		printf("ahem.  the delta file is empty?\n");
		exit(EXIT_FAILURE);
	    }
	    cptr=commands;
	}*/
    }
    printf("end was found(%u) at delta_pos(%lu), cptr(%lu), buff(%lu)\n", *cptr==0 ? 1 : 0, delta_pos,
	cptr - commands, buff_filled);
    printf("processed bytes(%lu) of bytes(%lu) available\n", delta_pos + (*cptr==0 ? 1: 0), delta_stat.st_size);
}
