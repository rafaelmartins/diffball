#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "gdiff.h"
//#include "cfile.h"
#include "bit-functions.h"

signed int gdiffEncodeDCBuffer(struct CommandBuffer *buffer, 
    unsigned int offset_type, unsigned char *ver, /*int fh*/  struct cfile *out_fh)
{
    unsigned char *ptr, clen;
    unsigned long fh_pos=0;
    //unsigned long offset;
    signed long s_off;
    unsigned long u_off;
    unsigned long delta_pos=0, dc_pos=0;
    unsigned long copies=0, adds_in_buff=0, adds_in_file=0;
    int lb, ob, off_is_sbytes;
    unsigned char type, out_buff[256];
    if(offset_type==ENCODING_OFFSET_VERS_POS || offset_type==ENCODING_OFFSET_DC_POS)
		off_is_sbytes=1;
    else
		off_is_sbytes=0;
    printf("commands in buffer(%lu)\n", buffer->count);
    buffer->lb_tail = buffer->lb_start;
    buffer->cb_tail = buffer->cb_head;
    buffer->cb_tail_bit = buffer->cb_head_bit;
    convertUBytesChar(out_buff, GDIFF_MAGIC, GDIFF_MAGIC_LEN);
    cwrite(out_fh, out_buff, GDIFF_MAGIC_LEN);
    //writeUBytes(fh, GDIFF_MAGIC, GDIFF_MAGIC_LEN);
    if(offset_type==ENCODING_OFFSET_START)
    	convertUBytesChar(out_buff, GDIFF_VER4, GDIFF_VER_LEN);
		//writeUBytes(fh, GDIFF_VER4, GDIFF_VER_LEN);
    else if(offset_type==ENCODING_OFFSET_VERS_POS)
		convertUBytesChar(out_buff, GDIFF_VER5, GDIFF_VER_LEN);
		//writeUBytes(fh, GDIFF_VER5, GDIFF_VER_LEN);
    else if(offset_type==ENCODING_OFFSET_DC_POS)
		convertUBytesChar(out_buff, GDIFF_VER6, GDIFF_VER_LEN);
		//writeUBytes(fh, GDIFF_VER6, GDIFF_VER_LEN);
    else {
		printf("wtf, gdiff doesn't know offset_type(%u). bug.\n",offset_type);
		exit(1);
    }
    cwrite(out_fh, out_buff, GDIFF_VER_LEN);
    while(buffer->count--){
		if((*buffer->cb_tail & (1 << buffer->cb_tail_bit))>0) {
	    	ptr=ver + buffer->lb_tail->offset;
	    	type=DC_COPY;
		} else if ((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==0){
		    type=DC_ADD;
		} else {
		    printf("wtf...\n");
		}
		switch(type)
		{
		case DC_ADD:
		    printf("add command, delta_pos(%lu), fh_pos(%lu), len(%lu), ",
				delta_pos, fh_pos, buffer->lb_tail->len);
		    adds_in_buff++;
		    u_off=buffer->lb_tail->len;
		    adds_in_file++;
		    if(buffer->lb_tail->len <= 246) {
				convertUBytesChar(out_buff, buffer->lb_tail->len, 1);
				cwrite(out_fh, out_buff, 1);
				//writeUBytes(fh, buffer->lb_tail->len, 1);
				delta_pos+=1;
				printf("type(%lu)\n", buffer->lb_tail->len);
		    } else if (buffer->lb_tail->len <= 0xffff) {
				//writeUBytes(fh, 247, 1);
				//writeUBytes(fh, buffer->lb_tail->len, 2);
				convertUBytesChar(out_buff, 247, 1);
				convertUBytesChar(out_buff + 1, buffer->lb_tail->len, 2);
				cwrite(out_fh, out_buff, 3);
				delta_pos+=3;
				printf("type(%u)\n", 247);
				printf("verifying, %u %u %u\n", out_buff[0], out_buff[1], out_buff[2]);
		    } else if (buffer->lb_tail->len <= 0xffffffff) {
				//writeUBytes(fh, 248, 1);
				//writeUBytes(fh, buffer->lb_tail->len, 4);
				convertUBytesChar(out_buff, 248, 1);
				convertUBytesChar(out_buff + 1, buffer->lb_tail->len, 4);
				cwrite(out_fh, out_buff, 5);
				delta_pos+=5;
				printf("type(%u)\n", 248);
				printf("verifying, %5.5s\n", out_buff);
		    } else {
				printf("wtf, encountered an offset larger then int size.  croaking.\n");
				exit(1);
		    }
		    //write(fh, ver + buffer->lb_tail->offset, buffer->lb_tail->len);
		    cwrite(out_fh, ver + buffer->lb_tail->offset, buffer->lb_tail->len);
		    delta_pos += buffer->lb_tail->len;
		    fh_pos += buffer->lb_tail->len;
		    break;
		case DC_COPY:
		    copies++;//clean this up.
		    if(off_is_sbytes) {
				if(offset_type==ENCODING_OFFSET_VERS_POS)
				    s_off = (signed long)buffer->lb_tail->offset - (signed long)fh_pos;
				else if(offset_type==ENCODING_OFFSET_DC_POS)
				    s_off = (signed long)buffer->lb_tail->offset - (signed long)dc_pos;		
				u_off = abs(s_off);
				ob=signedBytesNeeded(s_off);
		    } else {
				u_off = buffer->lb_tail->offset;
				ob=unsignedBytesNeeded(u_off);
		    }
		    lb=unsignedBytesNeeded(buffer->lb_tail->len);
		    if(lb> INT_BYTE_COUNT) {
				printf("wtf, too large of len in gdiff encoding. dieing.\n");
				exit(1);
		    }
		    if(ob > LONG_BYTE_COUNT) {
				printf("wtf, too large of offset in gdiff encoding. dieing.\n");
				exit(1);
		    }
		    clen=1;
		    if(lb <= BYTE_BYTE_COUNT)
				lb=BYTE_BYTE_COUNT;
		    else if(lb <= SHORT_BYTE_COUNT)
				lb=SHORT_BYTE_COUNT;
		    else
				lb=INT_BYTE_COUNT;
		    if(ob<=SHORT_BYTE_COUNT) {
				ob=SHORT_BYTE_COUNT;
				if(lb == BYTE_BYTE_COUNT)
				    out_buff[0]=249;
				else if(lb == SHORT_BYTE_COUNT)
				    out_buff[0]=250;
				else
				    out_buff[0]=251;
		    } else if (ob<=INT_BYTE_COUNT) {
				ob=INT_BYTE_COUNT;
				if(lb == BYTE_BYTE_COUNT)
			    	out_buff[0]=252;
				else if(lb == SHORT_BYTE_COUNT)
				    out_buff[0]=253;
				else
				    out_buff[0]=254;
		    } else {
				ob=LONG_BYTE_COUNT;
				out_buff[0]=255;
		    }
		    if(off_is_sbytes)
				convertSBytesChar(out_buff + clen, s_off, ob);
		    else 
				convertUBytesChar(out_buff + clen, u_off, ob);
		    clen+= ob;
		    convertUBytesChar(out_buff + clen, buffer->lb_tail->len, lb);
		    clen+=lb;
		    printf("writing copy command delta_pos(%lu), fh_pos(%lu), type(%u), offset(%ld), len(%lu)\n",
			delta_pos, fh_pos, out_buff[0], (off_is_sbytes ? s_off: u_off), buffer->lb_tail->len);
		    if(cwrite(out_fh, out_buff, clen)!=clen) {
				printf("shite, couldn't write copy command. eh?\n");
				exit(1);
		    }
		    fh_pos+=buffer->lb_tail->len;
		    delta_pos+=1 + ob + lb;
		    dc_pos += s_off;
		}
		DCBufferIncr(buffer);
    }
    convertUBytesChar(out_buff, 0, 1);
    cwrite(out_fh, out_buff, 1);
    //writeUBytes(fh, 0,1);
    printf("Buffer statistics- copies(%lu), adds(%lu)\n    copy ratio=(%f%%), add ratio(%f%%)\n",
	copies, adds_in_buff, ((float)copies)/((float)(copies + adds_in_buff))*100,
	((float)adds_in_buff)/((float)copies + (float)adds_in_buff)*100);
    printf("adds in file(%lu), average # of commands per add(%f)\n", adds_in_file,
	((float)adds_in_file)/((float)(adds_in_buff)));
    //ahem.  better error handling/returning needed. in time, in time...
    return 0;
}

signed int gdiffReconstructFile(int src_fh, int out_fh,
	/*struct PatchDeltaBuffer *PDBuff,*/ struct cfile *patchf,  
	unsigned int offset_type,
	unsigned int gdiff_version) {

    //unsigned char *cptr;
	const unsigned int buff_size = 4096;
    unsigned char buff[buff_size];
	unsigned long int len, x;
    unsigned long ver_pos=0, dc_pos=0;
    unsigned long int u_off;
    signed long int s_off;
    int off_is_sbytes, ob, lb;
    //unsigned char cpy_buff[PATCHER_COPY_BUFFER_SIZE];
    if(offset_type==ENCODING_OFFSET_VERS_POS || offset_type==ENCODING_OFFSET_DC_POS)
		off_is_sbytes=1;
    else if(offset_type==ENCODING_OFFSET_START)
		off_is_sbytes=0;
    else {
		printf("wtf, unknown offset_type for reconstruction(%u)\n",offset_type);
		exit(1);
    }
//    cptr= PDBuff->buffer;
	//	cread(patchf, &ccom, 1);
	printf("patchf->fh_pos(%lu)\n", patchf->fh_pos);
    while(cread(patchf, buff, 1)==1 && *buff != 0) {
	    //printf("ver_pos(%lu), fh_pos(%lu), command(%u)\n", ver_pos, patchf->fh_pos, *buff);
	    if(*buff > 0 && *buff <= 248) {
	        //add command
	        printf("add command\n");
	        if(*buff >=247 && *buff <= 248){
        		if (*buff==247)
	        	    lb=2;
				else if (*buff==248)
	        	    lb=4;
				if(cread(patchf, buff, lb)!=lb) {
					printf("shite, error reading.  \n");
					exit(1);
				}
	        	len= readUnsignedBytes(buff, lb);
	        	//cptr+=lb;
	        } else
	        	len=*buff;
	        //cptr++;
	        //printf("first byte of add command(%u) is (%u)\n", *(cptr -1), *cptr);
	        //printf("add  command delta_pos(%lu), fh_pos(%lu), len(%u)\n", PDBuff->delta_pos, fh_pos, ccom);
	        //clen = MIN(buff_filled - (cptr - commands), ccom);
	        //printf("len(%lu), clen(%lu)\n", *cptr, clen);
	        ver_pos += len;
	        while(len) {
	        	//x=MIN(len, PDBuff->filled_len - (cptr - PDBuff->buffer));
	        	x=MIN(len, buff_size);
	        	if(cread(patchf, buff, x)!=x) {
	        		printf("error reading in.\n");
	        		exit(1);
	        	}
				printf("   adding, len(%lu), x(%lu)\n", len, x);
	        	if((write(out_fh, buff, x))!=x) {
	        	    printf("Weird... error writing to out_fh\n");
	        	    exit(1);
	        	}
	        	len-=x;
	        }
	        //printf("left with cptr(%u)\n", *cptr);
	    } else if(*buff >= 249 ) {
	        //copy command
            printf("copy command ccom(%u) ", *buff);
	        if(*buff >=249 && *buff <= 251) {
				ob=2;
				if(*buff==249)
				    lb=1;
				else if(*buff==250)
				    lb=2;
				else if(*buff==251)
				    lb=4;
		    } else if (*buff >=252 && *buff <=254) {
				ob=4;
				if(*buff==252)
				    lb=1;
				else if(*buff==253)
				    lb=2;
				else 
				    lb=4;
	    	} else {
				ob=8;
				lb=4;
	    	}
	    	/*if(PDBuff->filled_len - (cptr - PDBuff->buffer) < 1 + ob + lb) {
				printf("refreshing buffer in copy, cptr(%u), filled_len(%u), (%u)\n",
		       PDBuff->filled_len, cptr - PDBuff->buffer,
		       PDBuff->filled_len - (cptr - PDBuff->buffer));
				refreshPDBuffer(PDBuff, PDBuff->filled_len - (cptr - PDBuff->buffer));
				cptr = PDBuff->buffer;
				printf("cptr(%u)\n", *cptr);
		    }*/
		    if(cread(patchf, buff + 1, lb + ob)!= lb + ob) {
		    	printf("error reading in lb/ob for copy...\n");
		    	exit(1);
		    }
		    //cptr++;
		    if(off_is_sbytes) {
				s_off=readSignedBytes(buff + 1, ob);
				//convertSBytesChar(out_buff + 1, s_off, ob);
		    } else {
				//convertUBytesChar(out_buff + 1, u_off, ob);
				u_off=readUnsignedBytes(buff + 1, ob);
		    }
		    //cptr+=ob;
		    len = readUnsignedBytes(buff + 1 + ob, lb);
		    printf("copy len(%lu)\n", len);
		    //cptr+=lb;
		    if(offset_type!=ENCODING_OFFSET_START) {
				if(offset_type==ENCODING_OFFSET_VERS_POS)
				    u_off = ver_pos + s_off;
				else //ENCODING_DC_POS
				    dc_pos = u_off = dc_pos + s_off;
		    }
		    if(lseek(src_fh, u_off, SEEK_SET)!= u_off) {
				printf("well that's weird, couldn't lseek.\n");
				exit(EXIT_FAILURE);
		    }
		    ver_pos+=len;
		    while(len) {
				x = MIN(buff_size, len);
				//printf("copying (%lu) bytes of len(%lu)\n", x ,len);
				if(read(src_fh, buff, x) != x) {
				    printf("hmm, error reading src_fh.\n");
				    //printf("clen(%u), len(%lu)\n", clen, len);
				    exit(EXIT_FAILURE);
				}
				if(write(out_fh, buff, x) != x){
				    printf("hmm, error writing the versionned file.\n");
				    exit(EXIT_FAILURE);
				}
				len -= x;
		    }
		}
		/*if(cptr - PDBuff->buffer == PDBuff->filled_len) {
		    printf("refreshing buffer\n");
		    //printf("refreshing buffer: cptr(%u)==buff_filled(%u)\n", 
		    cptr - commands, buff_filled);
		    refreshPDBuffer(PDBuff, 0);
		    cptr = PDBuff->buffer;
		    //continue;
		}*/
    }
    printf("closing command was (%u)\n", *buff);
    printf("cread fh_pos(%lu)\n", patchf->fh_pos); 
    printf("ver_pos(%lu)\n", ver_pos);
    return 0;
}
