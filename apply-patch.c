#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "cfile.h"
#include "dcbuffer.h"
#include "apply-patch.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))


void reconstructFile(struct CommandBuffer *dcbuff, struct cfile *src_cfh, 
	struct cfile *delta_cfh, struct cfile *out_cfh)
{
	//unsigned long int u_off;
	unsigned int const buff_size=4096;
	unsigned char buff[buff_size];
	unsigned long x, tmp;
	dcbuff->lb_tail = dcbuff->lb_start;
	dcbuff->cb_tail = dcbuff->cb_head;
	dcbuff->cb_tail_bit = dcbuff->cb_head_bit;
	while(dcbuff->count--){
		if((*dcbuff->cb_tail & (1 << dcbuff->cb_tail_bit))>0) {
		    //copies++;//clean this up.
		    printf("copy command, offset(%lu), len(%lu)\n",
		    dcbuff->lb_tail->offset, dcbuff->lb_tail->len);
		    cseek(src_cfh, dcbuff->lb_tail->offset, CSEEK_SET);
		    while(dcbuff->lb_tail->len) {
	        	x = MIN(dcbuff->lb_tail->len, buff_size);
	        	//printf("copying x(%u) of len(%u)\n", x, dcbuff->lb_tail->len);
	        	if(cread(src_cfh, buff, x)!=x) {
	        		printf("shite, couldn't read needed data from src_fh\n");
	        		exit(1);
	        	}
		        if((tmp=cwrite(out_cfh, buff, x))!=x) {
		   			printf("shite, couldn't only wrote(%lu) of (%lu) from the src_cfh to "
		   			"out_cfh\n", tmp, x);
		        	exit(1);
				}
				dcbuff->lb_tail->len -= x;
			}
		} else {
		    printf("add command, offset(%lu), len(%lu)\n", 
	        	dcbuff->lb_tail->offset, dcbuff->lb_tail->len);
		    cseek(delta_cfh, dcbuff->lb_tail->offset, CSEEK_SET);
	        while(dcbuff->lb_tail->len) {
	        	x = MIN(dcbuff->lb_tail->len, buff_size);
	        	if(cread(delta_cfh, buff, x)!=x) {
	        		printf("shite, couldn't read needed data from delta\n");
	        		exit(1);
	        	}
		        if(cwrite(out_cfh, buff, x)!=x) {
		        	printf("shite, couldn't write the needed data from the delta to out_fh\n");
		        	exit(1);
				}
				dcbuff->lb_tail->len -= x;
			}
		}
		DCBufferIncr(dcbuff);
    }
}
	
	