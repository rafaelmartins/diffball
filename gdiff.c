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
#include <stdlib.h>
#include "defs.h"
//#include <stdio.h>
//#include <assert.h>
#include "gdiff.h"
#include "bit-functions.h"

unsigned int
check_gdiff4_magic(cfile *patchf)
{
    unsigned char buff[GDIFF_MAGIC_LEN + 1];
    cseek(patchf, 0, CSEEK_FSTART);
    if(GDIFF_MAGIC_LEN != cread(patchf, buff, GDIFF_MAGIC_LEN)) {
	return 0;
    } else if(readUBytesBE(buff, GDIFF_MAGIC_LEN)!=GDIFF_MAGIC) {
	return 0;
    } else if (GDIFF_VER_LEN!=cread(patchf, buff, GDIFF_VER_LEN)) {
	return 0;
    } else if(readUBytesBE(buff, GDIFF_VER_LEN) == GDIFF_VER4_MAGIC) {
	return 2;
    }
    return 0;
}

unsigned int
check_gdiff5_magic(cfile *patchf)
{
    unsigned char buff[GDIFF_MAGIC_LEN + 1];
    cseek(patchf, 0, CSEEK_FSTART);
    if(GDIFF_MAGIC_LEN != cread(patchf, buff, GDIFF_MAGIC_LEN)) {
	return 0;
    } else if(readUBytesBE(buff, GDIFF_MAGIC_LEN)!=GDIFF_MAGIC) {
	return 0;
    } else if (GDIFF_VER_LEN!=cread(patchf, buff, GDIFF_VER_LEN)) {
	return 0;
    } else if(readUBytesBE(buff, GDIFF_VER_LEN) == GDIFF_VER5_MAGIC) {
	return 2;
    }
    return 0;
}

signed int 
gdiffEncodeDCBuffer(CommandBuffer *buffer, 
    unsigned int offset_type, cfile *ver_cfh, cfile *out_cfh)
{
    unsigned char /* *ptr,*/ clen;
    unsigned long fh_pos=0;
    signed long s_off=0;
    unsigned long u_off=0;
    unsigned long delta_pos=0, dc_pos=0;
    unsigned int lb=0, ob=0;
    unsigned char off_is_sbytes=0;
    unsigned char out_buff[5];
    unsigned long count;
    if(offset_type==ENCODING_OFFSET_VERS_POS || offset_type==ENCODING_OFFSET_DC_POS)
	off_is_sbytes=1;
    else
	off_is_sbytes=0;
    writeUBytesBE(out_buff, GDIFF_MAGIC, GDIFF_MAGIC_LEN);
    cwrite(out_cfh, out_buff, GDIFF_MAGIC_LEN);
    if(offset_type==ENCODING_OFFSET_START)
	writeUBytesBE(out_buff, GDIFF_VER4_MAGIC, GDIFF_VER_LEN);
    else if(offset_type==ENCODING_OFFSET_DC_POS)
	writeUBytesBE(out_buff, GDIFF_VER5_MAGIC, GDIFF_VER_LEN);
/*    else if(offset_type==ENCODING_OFFSET_DC_POS)
	writeUBytesBE(out_buff, GDIFF_VER6, GDIFF_VER_LEN);*/
    else {
	v2printf("wtf, gdiff doesn't know offset_type(%u). bug.\n",offset_type);
	exit(1);
    }
    cwrite(out_cfh, out_buff, GDIFF_VER_LEN);
    count = DCBufferReset(buffer);
    while(count--){
	if(buffer->lb_tail->len==0) {
	    DCBufferIncr(buffer);
	    continue;
	}
	if(current_command_type(buffer)==DC_ADD) {
	    v2printf("add command, delta_pos(%lu), fh_pos(%lu), len(%lu)\n",
		delta_pos, fh_pos, buffer->lb_tail->len);
	    u_off=buffer->lb_tail->len;
	    if(buffer->lb_tail->len <= 246) {
		out_buff[0] = buffer->lb_tail->len;
		cwrite(out_cfh, out_buff, 1);
		delta_pos+=1;
	    } else if (buffer->lb_tail->len <= 0xffff) {
		out_buff[0] = 247;
		writeUBytesBE(out_buff + 1, buffer->lb_tail->len, 2);
		cwrite(out_cfh, out_buff, 3);
		delta_pos+=3;
	    } else if (buffer->lb_tail->len <= 0xffffffff) {
		out_buff[0] = 248;
		writeUBytesBE(out_buff + 1, buffer->lb_tail->len, 4);
		cwrite(out_cfh, out_buff, 5);
		delta_pos+=5;
	    } else {
		v2printf("wtf, encountered an offset larger then int size.  croaking.\n");
		exit(1);
	    }
	    if(buffer->lb_tail->len != 
		copy_cfile_block(out_cfh, ver_cfh, buffer->lb_tail->offset,
		buffer->lb_tail->len)) 
		abort();

	    delta_pos += buffer->lb_tail->len;
	    fh_pos += buffer->lb_tail->len;
	} else {
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
		v2printf("wtf, too large of len in gdiff encoding. dieing.\n");
		exit(1);
	    }
	    if(ob > LONG_BYTE_COUNT) {
		v2printf("wtf, too large of offset in gdiff encoding. dieing.\n");
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
		writeSBytesBE(out_buff + clen, s_off, ob);
	    else 
		writeUBytesBE(out_buff + clen, u_off, ob);
	    clen+= ob;
	    writeUBytesBE(out_buff + clen, buffer->lb_tail->len, lb);
	    clen+=lb;
	    v2printf("copy delta_pos(%lu), fh_pos(%lu), type(%u), offset(%ld), len(%lu)\n",
		delta_pos, fh_pos, out_buff[0], (off_is_sbytes ? s_off: u_off), buffer->lb_tail->len);
	    if(cwrite(out_cfh, out_buff, clen)!=clen) {
		v2printf("shite, couldn't write copy command. eh?\n");
		exit(1);
	    }
	    fh_pos+=buffer->lb_tail->len;
	    delta_pos+=1 + ob + lb;
	    dc_pos += s_off;
	}
	DCBufferIncr(buffer);
    }
    out_buff[0] = 0;
    cwrite(out_cfh, out_buff, 1);
    //ahem.  better error handling/returning needed. in time, in time...
    return 0;
}

signed int 
gdiffReconstructDCBuff(cfile *patchf, CommandBuffer *dcbuff, 
	unsigned int offset_type)
{
    const unsigned int buff_size = 5;
    unsigned char buff[buff_size];
    unsigned long int len;
    unsigned long ver_pos=0, dc_pos=0;
    unsigned long int u_off;
    signed long int s_off;
    int off_is_sbytes, ob, lb;
    if(offset_type==ENCODING_OFFSET_VERS_POS || offset_type==ENCODING_OFFSET_DC_POS)
	off_is_sbytes=1;
    else if(offset_type==ENCODING_OFFSET_START)
	off_is_sbytes=0;
    else {
	v2printf("wtf, unknown offset_type for reconstruction(%u)\n",offset_type);
	exit(1);
    }
    cseek(patchf, 5, CSEEK_CUR);
    while(cread(patchf, buff, 1)==1 && *buff != 0) {
	if(*buff > 0 && *buff <= 248) {
	    //add command
	    v2printf("add command type(%u) ", *buff);
	    if(*buff >=247 && *buff <= 248){
        	if (*buff==247)
	            lb=2;
		else if (*buff==248)
		    lb=4;
		if(cread(patchf, buff, lb)!=lb) {
		    v2printf("shite, error reading.  \n");
		    exit(1);
		}
	        len= readUBytesBE(buff, lb);
	    } else
		len=*buff;
	    DCBufferAddCmd(dcbuff, DC_ADD, ctell(patchf, CSEEK_FSTART), len);
	    cseek(patchf, len, CSEEK_CUR);
	} else if(*buff >= 249 ) {
	    //copy command
            v2printf("copy command ccom(%u) ", *buff);
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
		if(cread(patchf, buff + 1, lb + ob)!= lb + ob) {
		     v2printf("error reading in lb/ob for copy...\n");
		     exit(1);
		}
		if(off_is_sbytes) {
		     s_off=readSBytesBE(buff + 1, ob);
		} else {
		     u_off=readUBytesBE(buff + 1, ob);
		}
		len = readUBytesBE(buff + 1 + ob, lb);
		if(offset_type!=ENCODING_OFFSET_START) {
		    if(offset_type==ENCODING_OFFSET_VERS_POS)
			u_off = ver_pos + s_off;
		    else //ENCODING_DC_POS
			dc_pos = u_off = dc_pos + s_off;
		}
		v2printf("offset(%lu), len(%lu)\n", u_off, len);
		DCBufferAddCmd(dcbuff, DC_COPY, u_off, len);
		ver_pos+=len;
	    }
    }
    v2printf("closing command was (%u)\n", *buff);
    v2printf("cread fh_pos(%lu)\n", ctell(patchf, CSEEK_ABS)); 
    v2printf("ver_pos(%lu)\n", ver_pos);
    return 0;
}
