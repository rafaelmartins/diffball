#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include "cfile.h"

unsigned int verbosity;

int main(int argc, char **argv){
    struct stat fstat;
    int fh, wfh;
    cfile cfh_orig, *cfh;
    cfh = &cfh_orig;
    unsigned char buff[512];
    if(argc < 2 || stat(argv[1], &fstat)) {
	printf("stat failed || file not found\n");
	abort();
    }
    if((fh=open(argv[1], O_RDONLY,0)) == -1) {
	printf("failed opening file...\n");
	abort();
    }
    copen(cfh, fh, 0, fstat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    /*if(fstat.st_size!=cread(cfh, buff, fstat.st_size)) {
	printf("didn't read all of the file..\n");
	abort();
    }*/
    assert(0==cseek(cfh, 0, CSEEK_FSTART));
    cread(cfh, buff, 1);
    assert(48==buff[0]);
    cread(cfh, buff, 1);
    assert('1'==buff[0]);
    assert(ctell(cfh, CSEEK_ABS)==2);
    assert(3==cseek(cfh, 1, CSEEK_CUR));
    assert(1==cseek(cfh, -2, CSEEK_CUR));
    assert(cfile_len(cfh)==8);
    cclose(cfh);
    copen(cfh, fh, 1, fstat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    assert(cseek(cfh, 0, CSEEK_FSTART)==0);
    assert(cseek(cfh, 1, CSEEK_ABS)==1);
    assert(ctell(cfh, CSEEK_FSTART)==0);
    assert(ctell(cfh, CSEEK_ABS)==1);
    assert(cfile_len(cfh)==7);
    assert(ctell(cfh, CSEEK_END)==7);
    assert(7==cread(cfh, buff, 100));
    assert(0==ctell(cfh, CSEEK_END));
    printf("beginning intrusive tests\n");
    cclose(cfh);
    copen(cfh, fh, 0, fstat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    cfh->data.size=2;
    assert(4==cseek(cfh, 4, CSEEK_CUR));
    assert(0==cseek(cfh, -4, CSEEK_CUR));
    assert(2==cread(cfh, buff, 2));
    assert(1==cread(cfh, buff, 1));
    assert(cfh->data.offset==2);
    assert(cfh->data.end==2);
    assert(cfh->data.pos==1);
    assert(ctell(cfh, CSEEK_ABS)==3);
    printf("cseeking past buffer break\n");
    assert(cseek(cfh, -3, CSEEK_CUR)==0);
    assert(cfh->data.pos==0);
    assert(cfh->data.offset==0);
//    assert(cfh->state_flags & CFILE_SEEK_NEEDED);
    assert(2==cread(cfh, buff, 2));
    assert('0'==buff[0]);
    assert('1'==buff[1]);
    assert(cfh->state_flags==0);
    assert(cseek(cfh, 4, CSEEK_FSTART)==4);
    assert(4==cread(cfh, buff, 5));
    assert('4' == buff[0]);
    printf("past all read tests that I've coded so far...\n");
    cclose(cfh);
    //close(fh);
    printf("so starts write.\n");
    strcpy(buff, "write-test");
    if((wfh = open(buff, O_WRONLY | O_TRUNC | O_CREAT, 0644))== -1) {
	printf("kind of hard to test, since can't open a fucking file..\n");
	exit(1);
    }
    copen(cfh, wfh, 0,0, NO_COMPRESSOR, CFILE_WONLY);
    assert(cwrite(cfh, buff, strlen(buff))==strlen(buff));
    printf("begining low level fucking with write\n");
    cfh->data.size=strlen(buff);
    assert(cwrite(cfh, buff, 2)==2);
    assert(cfh->data.offset==strlen(buff));
    assert(cfh->data.pos==2);
    assert(ctell(cfh, CSEEK_ABS) == strlen(buff) + 2);
    cclose(cfh);
    close(wfh);
    printf("beginning another round of cseek testing\n");
    copen(cfh, fh, 0, fstat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    assert(2==cseek(cfh, 2, CSEEK_FSTART));
    assert(2==cfh->data.offset + cfh->data.pos);
    assert(2==cread(cfh, buff, 2));
    assert('2'==buff[0]);
    assert('3'==buff[1]);
    cclose(cfh);
    printf("starting window tests\n");
    copen(cfh, fh, 2, fstat.st_size, NO_COMPRESSOR,  CFILE_RONLY);
    assert(2==cread(cfh, buff,2));
    assert('2'==buff[0]);
    assert('3'==buff[1]);
    assert(1==cseek(cfh, -1, CSEEK_CUR));
    assert(1==cread(cfh, buff,1));
    assert('3'==buff[0]);
    printf("starting md5 test.\n");
    cclose(cfh);
    copen(cfh, fh, 0, fstat.st_size, NO_COMPRESSOR, CFILE_RONLY | 
	CFILE_COMPUTE_MD5);
    cfh->data.size =2;
    assert(cseek(cfh, 2, CSEEK_CUR)==2);
    assert(cfh->data_md5_pos == 0);
    assert(2==cread(cfh, buff, 2));
    assert(cfh->data_md5_pos==0);
    assert(0==cseek(cfh, 0, CSEEK_FSTART));
    assert(2==cread(cfh, buff, 2));
    assert(cfh->data_md5_pos == cfh->data.end);
    //assert(fstat.st_size==cread(cfh, buff, fstat.st_size));
    cfile_finalize_md5(cfh);
    memcpy(buff, cfh->data_md5, 16);
    unsigned int x;
    printf("md5=\n");
    for(x=0; x < 16; x++) 
	printf("%2.2x", buff[x]);
    printf("\n");
    assert(0x43==buff[0] && 0x2b==buff[1] && 0xab==buff[2] && 0x3b==buff[3] &&
	0xaf==buff[4] && 0x2c==buff[5] && 0x0c==buff[6] && 0xb7==buff[7] &&
	0x50==buff[8] && 0xde==buff[9] && 0xee==buff[10] && 0x93==buff[11] &&
	0xa6==buff[12] && 0xd5==buff[13] && 0x19==buff[14] && 0x15==buff[15]);
    printf("correct md5.\n");
    cclose(cfh);
    printf("beginning page tests\n");
    copen(cfh, fh, 0, fstat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    cfh->data.size=2;
    cfile_window *cfw;
    cfw = expose_page(cfh);
    assert(cfw!=NULL);
    assert('0'==cfw->buff[0]);
    assert('1'==cfw->buff[1]);
    assert(0==cfw->offset);
    cfw=next_page(cfh);
    assert(cfw!=NULL);
    assert('2'==cfw->buff[0]);
    assert('3'==cfw->buff[1]);
    assert(cfw->size==2);
    assert(cfw->end==2);
    assert(cfw->offset==2);
    cfw=prev_page(cfh);
    assert(cfw!=NULL);
    assert('0'==cfw->buff[0]);
    assert('1'==cfw->buff[1]);
    assert(0==cfw->offset);
    printf("so ends the tests.  Prob still didn't find the bug, eh?\n");
    return 0;
}
