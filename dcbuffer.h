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
#ifndef _HEADER_DCBUFFER
#define _HEADER_DCBUFFER 1

#include "cfile.h"

extern unsigned int global_use_md5;

#define DC_ADD				0x0
#define DC_COPY				0x1
#define ENCODING_OFFSET_START		0x0
#define ENCODING_OFFSET_VERS_POS	0x1
#define ENCODING_OFFSET_DC_POS		0x2

/* type for CommandBuffer.DCBtype */
#define DCBUFFER_FULL_TYPE		0x1
#define DCBUFFER_MATCHES_TYPE		0x2
#define DCBUFFER_PASSES_TYPE		0x3

#define ADD_CFH_FREE_FLAG		0x1

typedef struct {
    unsigned long offset;
    unsigned long len;
} DCLoc;

typedef struct {
    unsigned long src_pos;
    unsigned long ver_pos;
    unsigned long len;
} DCLoc_match;



typedef struct {
    unsigned long src_size;
    unsigned long ver_size;
    unsigned long reconstruct_pos;
    unsigned char DCBtype;
    union {
	struct {
	    unsigned long buffer_count;
	    unsigned long buffer_size;
	    unsigned char *cb_start, *cb_end, *cb_head, *cb_tail;
	    unsigned char cb_tail_bit, cb_head_bit;
	    DCLoc *lb_start, *lb_end, *lb_head, *lb_tail;
	} full;
	struct {
	    unsigned long buff_pos;
	    unsigned long buff_count;
	    unsigned long buff_size;
	    DCLoc_match *buff;
	} matches;
	struct {
	    DCLoc_match **buff;
	    unsigned long pass_count;
	    unsigned long *buff_count;
	} multipass;
    } DCB;
    cfile *add_cfh;
    unsigned long flags;
} CommandBuffer;


#define DCBF_cur_len(buff)			\
    ((buff)->DCB.full.lb_tail->len)
#define DCBF_cur_off(buff)			\
    ((buff)->DCB.full.lb_tail->offset)
#define DCBUFFER_REGISTER_ADD_CFH(buff, handle)		\
    (buff)->add_cfh = (handle)
#define DCBUFFER_FREE_ADD_CFH_FLAG(buff) (buff)->flags |= ADD_CFH_FREE_FLAG;

unsigned long inline current_command_type(CommandBuffer *buff);
void DCBufferIncr(CommandBuffer *buffer);
void DCBufferDecr(CommandBuffer *buffer);
void DCBufferCollapseAdds(CommandBuffer *buffer);
void DCBufferAddCmd(CommandBuffer *buffer, int type, unsigned long offset, unsigned long len);
void DCBufferTruncate(CommandBuffer *buffer, unsigned long len);
void DCBufferFree(CommandBuffer *buffer);
void DCBufferInit(CommandBuffer *buffer, unsigned long buffer_size,
    unsigned long src_size, unsigned long ver_size, unsigned char type);
unsigned long DCBufferReset(CommandBuffer *buffer);

void DCB_resize_full(CommandBuffer *buffer);
void DCB_resize_matches(CommandBuffer *buffer);
#endif
