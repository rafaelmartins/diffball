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
#include <string.h>
#include <assert.h>
#include "udiff.h"

unsigned long
getUDec(cfile *cfh)
{
    unsigned long num=0;
    unsigned char b;
    assert(1==cread(cfh, &b, 1));
//    printf("get %c\n",b);
    while(b >= '0' && b <= '9') {
	num *= 10;
	num += (b-'0');
	cread(cfh, &b, 1);
//	printf("got %c\n", b);
    }
//    printf("num was %lu\n", num);
    cseek(cfh, -1, CSEEK_CUR);
    return num;
}

signed int
udiffReconstructDCBuff(cfile *patchf, cfile *src_cfh,
    struct tar_entry **tarball, CommandBuffer *dcbuff)
{
    unsigned long s_line, s_len, v_line, v_len;
    unsigned long s_lastline, s_lastoff;
    unsigned long line_count, offset, len;
    unsigned char add_copy, newline_complaint=0;
    unsigned char buff[512];
    cread(patchf, buff, 3);
    assert(0==memcmp(buff, "---",3));
    if(2!=skip_lines_forward(patchf, 2))
	abort();
    s_lastoff = 0;
    s_lastline = 1;
    while(cfile_len(patchf)!= ctell(patchf, CSEEK_FSTART)) {
//	printf("pos=%lu\n", patchf->data.pos);
	cread(patchf, buff, 4);
	if('\\'==buff[0]) {
	    printf("got me a (hopefully) 'No newline...'\n");
	    assert(22==cread(patchf, buff + 4, 22));
	    assert(memcmp(buff,"\\ No newline at end of file", 26)==0);
	    skip_lines_forward(patchf,1);
	    /* hokay. this is a kludge, not even innovative/good enough to be 
	       called a hack. */
	    assert(newline_complaint);
	    DCBufferTruncate(dcbuff,1);
	    continue;
	}
	assert(0==memcmp(buff, "@@ -",4));
	s_line = getUDec(patchf);
	printf("for segment, s_line(%lu), s_lastline(%lu):", s_line, 
	    s_lastline);
	skip_lines_forward(src_cfh, s_line - s_lastline);
	s_lastline = s_line;
	printf("at pos(%lu) in src_cfh\n", ctell(src_cfh, CSEEK_FSTART));
	//printf("s_line=%lu\n", s_line);
	cread(patchf, buff, 1);
	if(buff[0]==',')
	    s_len = /*s_line -*/ getUDec(patchf);
	else
	    s_len = 1;
	assert(2==cread(patchf,buff,2));
	assert(0==memcmp(buff, " +",2));
	v_line = getUDec(patchf);
	cread(patchf, buff, 1);
	if(buff[0]==',')
	    v_len = /*v_line -*/ getUDec(patchf);
	else
	    v_len = 1;
	skip_lines_forward(patchf, 1);
	line_count=0;
	add_copy=1;
	/* now handle the specific segment data. */
	while(line_count < v_len) {
//	    printf("line_count=%lu, v_len=%lu\n", line_count + 1, v_len);
	    cread(patchf, buff, 1);
	    assert(' '==buff[0] || '+'==buff[0] || '-'==buff[0] || 
		'\\'==buff[0]);
	    if(' '==buff[0]) {
		printf("got a common  line( ): ");
		/* common line */
		printf("skipping a line in both src and patch\n");
		if(add_copy==0) 
		    s_lastoff = ctell(src_cfh, CSEEK_FSTART);
		line_count++;
		skip_lines_forward(src_cfh, 1);
		s_lastline++;
		skip_lines_forward(patchf, 1);
		add_copy=1;
	    } else if('+'==buff[0]) {
		printf("got a version tweak(+): ");
		if(add_copy) {
		    offset = ctell(src_cfh, CSEEK_FSTART);
		    printf("adding copy for add_copy: ");
		    DCBufferAddCmd(dcbuff, DC_COPY, s_lastoff, 
			offset - s_lastoff);
		    s_lastoff = offset;
		    //s_lastline++; 
		    add_copy=0;
		}
		offset = ctell(patchf, CSEEK_FSTART);
		printf("skipping a line in patch\n");
		skip_lines_forward(patchf, 1);
		len = (ctell(patchf, CSEEK_FSTART)) - offset;
		DCBufferAddCmd(dcbuff, DC_ADD, offset, len);
		line_count++;
	    } else if('-'==buff[0]) {
		printf("got a source  tweak(-): ");
		if(add_copy) {
		    offset = ctell(src_cfh, CSEEK_FSTART);
		    printf("adding copy for add_copy: ");
		    DCBufferAddCmd(dcbuff, DC_COPY, s_lastoff, 
			offset - s_lastoff);
		    add_copy=0;
		}
		printf("skipping a line in patch\n");
		skip_lines_forward(patchf, 1);
		s_lastline++;
		skip_lines_forward(src_cfh, 1);
		s_lastoff = ctell(src_cfh, CSEEK_FSTART);
	    } else if('\\'==buff[0]) {
		printf("got me a (hopefully) 'No newline...'\n");
		assert(26==cread(patchf, buff + 1, 26));
		assert(memcmp(buff,"\\ No newline at end of file", 26)==0);
		skip_lines_forward(patchf,1);
		/* kludge.  May be a wrong assumption, but diff's complaints 
		   about lack of newline at the end of a file should first be 
		   for the source file.  This little puppy is used for that */
		newline_complaint=1;
	    }
	} 
	printf("so ends that segment\n");
    }
    if(ctell(src_cfh, CSEEK_FSTART)!=cfile_len(src_cfh))
	DCBufferAddCmd(dcbuff, DC_COPY, s_lastoff, cfile_len(src_cfh) - s_lastoff);
    return 0;
}
