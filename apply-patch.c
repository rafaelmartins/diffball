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
		//printf("handling command(%lu)\n", dcbuff->count + 1);
		/*if((*dcbuff->cb_tail & (1 << dcbuff->cb_tail_bit))>0) {
	    	//ptr=ver + dcbuff->lb_tail->offset;
	    	type=DC_COPY;
		} else if ((*dcbuff->cb_tail & (1 << dcbuff->cb_tail_bit))==0){
		    type=DC_ADD;
		} else {
		    printf("wtf...\n");
		}
		switch(type)
		{
		case DC_ADD:*/
		//if(((*dcbuff->cb_tail >> dcbuff->cb_tail_bit) & ~0x1) == DC_ADD) {
		//case DC_COPY:
		if((*dcbuff->cb_tail & (1 << dcbuff->cb_tail_bit))>0) {
		    //copies++;//clean this up.
		    printf("copy command, offset(%lu), len(%lu)\n",
		    dcbuff->lb_tail->offset, dcbuff->lb_tail->len);
		    /*printf("copy cseeking to (%lu), cur_offset(%lu),",
		    // cpos(%lu), len(%lu)\n", 
		    dcbuff->lb_tail->offset, cposition(src_cfh));
		    printf(" cseek(%lu),", 
		    cseek(src_cfh, dcbuff->lb_tail->offset, CSEEK_SET));
		    printf(" cpos(%lu),", cposition(src_cfh));
		    printf(" len(%lu)\n", dcbuff->lb_tail->len);*/
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
		//if((*dcbuff->cb_tail * (1 << dcbuff->cb_tail_bit))==DC_ADD){
		    //printf("add command, delta_pos(%lu), fh_pos(%lu), len(%lu), ",
			//	delta_pos, fh_pos, dcbuff->lb_tail->len);
		    //adds_in_buff++;
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
			//DCBufferAddCmd(dcbuff, DC_ADD, cposition(patchf), len);
			//cseek(patchf, len);
		//} else {
		
		}
		DCBufferIncr(dcbuff);
    }
}
	
	