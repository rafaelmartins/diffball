/*
  Copyright (C) 2003-2004 Brian Harring

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
#include "defs.h"
#include <string.h>
#include "udiff.h"

unsigned long
getUDec(cfile *cfh)
{
    unsigned long num=0;
    unsigned char b;
    /* need error control on this one boys */
    cread(cfh, &b, 1);
    while(b >= '0' && b <= '9') {
	num *= 10;
	num += (b-'0');
	cread(cfh, &b, 1);
    }
    cseek(cfh, -1, CSEEK_CUR);
    return num;
}

signed int
udiffReconstructDCBuff(cfile *ref_cfh, cfile *patchf,
    tar_entry **tarball, CommandBuffer *dcbuff)
{
    unsigned long s_line, s_len, v_line, v_len;
    unsigned long s_lastline, s_lastoff;
    unsigned long line_count, offset, len;
    unsigned char add_copy, newline_complaint=0;
    unsigned char add_id, ref_id;
    unsigned char buff[512];
    cread(patchf, buff, 3);
    if(0!=memcmp(buff, "---", 3))
	return PATCH_CORRUPT_ERROR;
    if(2!=skip_lines_forward(patchf, 2))
	return PATCH_CORRUPT_ERROR;
    s_lastoff = 0;
    s_lastline = 1;
    add_id = DCB_REGISTER_ADD_SRC(dcbuff, patchf, NULL, 0);
    ref_id = DCB_REGISTER_COPY_SRC(dcbuff, ref_cfh, NULL, 0);
    while(cfile_len(patchf)!= ctell(patchf, CSEEK_FSTART)) {
	cread(patchf, buff, 4);
	if('\\'==buff[0]) {
	    v2printf("got me a (hopefully) 'No newline...'\n");
	    if(22!=cread(patchf, buff + 4, 22))
		return EOF_ERROR;
	    if(0!=memcmp(buff, "\\ No newline at end of file", 26))
		return PATCH_CORRUPT_ERROR;
	    skip_lines_forward(patchf,1);
	    /* hokay. this is a kludge, not even innovative/good enough to be 
	       called a hack. */
	    assert(newline_complaint);
	    DCB_truncate(dcbuff,1);
	    continue;
	} else if ('B'==buff[0]) {
	    /* handle the "Binary file x and y differ" by puking. */
	    return PATCH_CORRUPT_ERROR;
	}
	if(0!=memcmp(buff, "@@ -", 4))
	    return PATCH_CORRUPT_ERROR;
	s_line = getUDec(patchf);
	v2printf("for segment, s_line(%lu), s_lastline(%lu):", s_line, 
	    s_lastline);
	skip_lines_forward(ref_cfh, s_line - s_lastline);
	s_lastline = s_line;
	v2printf("at pos(%lu) in ref_cfh\n", ctell(ref_cfh, CSEEK_FSTART));
	cread(patchf, buff, 1);
	if(buff[0]==',')
	    s_len = /*s_line -*/ getUDec(patchf);
	else
	    s_len = 1;
	if(2!=cread(patchf, buff, 2))
	    return EOF_ERROR;
	if(0!=memcmp(buff, " +", 2))
	    return PATCH_CORRUPT_ERROR;
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
	    cread(patchf, buff, 1);
	    assert(' '==buff[0] || '+'==buff[0] || '-'==buff[0] || 
		'\\'==buff[0]);
	    if(' '==buff[0]) {
		v2printf("got a common  line( ): ");
		/* common line */
		v2printf("skipping a line in both src and patch\n");
		if(add_copy==0) 
		    s_lastoff = ctell(ref_cfh, CSEEK_FSTART);
		line_count++;
		skip_lines_forward(ref_cfh, 1);
		s_lastline++;
		skip_lines_forward(patchf, 1);
		add_copy=1;
	    } else if('+'==buff[0]) {
		v2printf("got a version tweak(+): ");
		if(add_copy) {
		    offset = ctell(ref_cfh, CSEEK_FSTART);
		    v2printf("adding copy for add_copy: ");
		    DCB_add_copy(dcbuff, s_lastoff, 0, offset - s_lastoff, ref_id);
		    s_lastoff = offset;
		    //s_lastline++; 
		    add_copy=0;
		}
		offset = ctell(patchf, CSEEK_FSTART);
		v2printf("skipping a line in patch\n");
		skip_lines_forward(patchf, 1);
		len = (ctell(patchf, CSEEK_FSTART)) - offset;
		DCB_add_add(dcbuff, offset, len, add_id);
		line_count++;
	    } else if('-'==buff[0]) {
		v2printf("got a source  tweak(-): ");
		if(add_copy) {
		    offset = ctell(ref_cfh, CSEEK_FSTART);
		    v2printf("adding copy for add_copy: ");
		    DCB_add_copy(dcbuff, s_lastoff, 0, offset - s_lastoff, ref_id);
		    add_copy=0;
		}
		v2printf("skipping a line in patch\n");
		skip_lines_forward(patchf, 1);
		s_lastline++;
		skip_lines_forward(ref_cfh, 1);
		s_lastoff = ctell(ref_cfh, CSEEK_FSTART);
	    } else if('\\'==buff[0]) {
		v2printf("got me a (hopefully) 'No newline...'\n");
		if(26!=cread(patchf, buff + 1, 26))
		    return EOF_ERROR;
		if(0!=memcmp(buff, "\\ No newline at end of file", 26))
		    return PATCH_CORRUPT_ERROR;
		skip_lines_forward(patchf,1);
		/* kludge.  May be a wrong assumption, but diff's complaints 
		   about lack of newline at the end of a file should first be 
		   for the source file.  This little puppy is used for that */
		newline_complaint=1;
	    } else {
		return PATCH_CORRUPT_ERROR;
	    }
	} 
	v2printf("so ends that segment\n");
    }
    if(ctell(ref_cfh, CSEEK_FSTART)!=cfile_len(ref_cfh))
	DCB_add_copy(dcbuff, s_lastoff, 0, cfile_len(ref_cfh) - s_lastoff, ref_id);
    return 0;

}
