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
#include "defs.h"
#include "config.h"

extern unsigned int global_use_md5;

#define DC_ADD				0x0
#define DC_COPY				0x1
#define ENCODING_OFFSET_START		0x0
#define ENCODING_OFFSET_VERS_POS	0x1
#define ENCODING_OFFSET_DC_POS		0x2

/* type for CommandBuffer.DCBtype */
#define DCBUFFER_FULL_TYPE		0x1
#define DCBUFFER_MATCHES_TYPE		0x2
#define DCBUFFER_LLMATCHES_TYPE		0x4

#define ADD_CFH_FREE_FLAG		0x1
#define DCB_LLM_FINALIZED		0x2
#define EXTRA_PATCH_DATA_USED		0x4

// internal dcbuffer macros.
#define LLM_VEND(l)  ((l)->ver_pos + (l)->len)

typedef struct {
    off_u64 offset;
    unsigned long len;
} DCLoc;

typedef struct {
    off_u64 src_pos;
    off_u64 ver_pos;
    unsigned long len;
} DCLoc_match;

typedef struct LL_DCLmatch LL_DCLmatch;

struct LL_DCLmatch {
    off_u64 src_pos, ver_pos;
    unsigned long len;
    LL_DCLmatch *next;
};

typedef struct {
    DCLoc loc;
    unsigned long src_id;
    unsigned char type;
} DCommand;

typedef struct _CommandBuffer *DCB_ptr;
typedef unsigned long (*dcb_src_func)(DCB_ptr, DCommand *, cfile *);

typedef struct _CommandBuffer {
    off_u64 src_size;
    off_u64 ver_size;
    off_u64 reconstruct_pos;
    unsigned char DCBtype;
#ifdef DEBUG_DCBUFFER
    off_u64 total_copy_len;
#endif
    union {
	struct {
	    unsigned long buffer_count;
	    unsigned long buffer_size;
	    unsigned char *cb_start, *cb_end, *cb_head, *cb_tail;
	    unsigned char cb_tail_bit, cb_head_bit;
	    DCLoc *lb_start, *lb_end, *lb_head, *lb_tail;
	    unsigned long add_index;
	    unsigned char *add_src_id;
	} full;
	struct {
	    off_u64 ver_start;
	    unsigned long buff_count, buff_size;
	    DCLoc_match *buff, *cur;
	} matches;
	struct {
	    off_u64 ver_start, gap_pos;
	    LL_DCLmatch *main_head, *main;
	    unsigned long buff_count, buff_size, main_count;
	    LL_DCLmatch *buff, *cur;
	    void **free;
	    unsigned long free_size, free_count;
	} llm;
    } DCB;
    cfile *add_src_cfh[2];
    dcb_src_func add_src_func[2];
    unsigned short add_src_count;

    /* this is a hack, and not a particularly good one either.
	things need to be expanded to eliminate the need for this- check the 
	bsdiff format reconstructor if you're curious of it's reason for 
	existing*/
    void *extra_patch_data;
    cfile *copy_src_cfh[2];
    dcb_src_func copy_src_func[2];
    unsigned short copy_src_count;
    unsigned long flags;
} CommandBuffer;

#ifdef DEV_VERSION
#define DCB_REGISTER_EXTRA_PATCH_DATA(dcbuff, ext_ptr)			\
    if((dcbuff)->extra_patch_data != NULL){				\
	abort();							\
    }									\
    (dcbuff)->extra_patch_data = (void *)(ext_ptr)
#else
#define DCB_REGISTER_EXTRA_PATCH_DATA(dcbuff, ext_ptr)			\
    (dcbuff)->extra_patch_data = (void *)(ext_ptr)
#endif

#define copyDCB_add_src(buff, dc, out_cfh)				\
    (buff)->add_src_func[(dc)->src_id]((buff), (dc), (out_cfh))
#define copyDCB_copy_src(buff, dc, out_cfh)				\
    (buff)->copy_src_func[(dc)->src_id]((buff), (dc), (out_cfh))

#define DCBUFFER_REGISTER_ADD_SRC(buff, handle, func)			\
    if((buff)->add_src_count >= 2){abort();};				\
    (buff)->add_src_cfh[(buff)->add_src_count] = (handle);		\
    (buff)->add_src_func[(buff)->add_src_count] = ((func) == NULL ? 	\
	&default_dcb_add_func : (func));				\
    (buff)->add_src_count++

#define DCBUFFER_REGISTER_COPY_SRC(buff, handle, func)			\
    if((buff)->copy_src_count >= 2) {abort();};				\
    (buff)->copy_src_cfh[(buff)->copy_src_count] = (handle);		\
    (buff)->copy_src_func[(buff)->copy_src_count] = ((func) == NULL ? 	\
	&default_dcb_copy_func : (func));				\
    (buff)->copy_src_count++

/* not used anymore, chuck at some point */
#define DCB_REGISTER_MATCHES_VER_CFH(buff, cfh)				\
    if((buff)->DCBtype==DCBUFFER_MATCHES_TYPE) {			\
	(buff)->DCB.matches.ver_start = cfile_start_offset((cfh));	\
    } else if((buff)->DCBtype==DCBUFFER_LLMATCHES_TYPE) {		\
	(buff)->DCB.llm.ver_start = cfile_start_offset((cfh));		\
    }

#define DCBUFFER_FREE_ADD_CFH_FLAG(buff) (buff)->flags |= ADD_CFH_FREE_FLAG;

unsigned long inline current_command_type(CommandBuffer *buff);
unsigned long default_dcb_add_func(CommandBuffer *dcb, DCommand *dc, 
    cfile *out_cfh);
unsigned long default_dcb_copy_func(CommandBuffer *dcb, DCommand *dc,
    cfile *out_cfh);
void DCBufferIncr(CommandBuffer *buffer);
void DCBufferDecr(CommandBuffer *buffer);
void DCBufferCollapseAdds(CommandBuffer *buffer);
void DCBufferFree(CommandBuffer *buffer);
void DCBufferInit(CommandBuffer *buffer, unsigned long buffer_size,
    unsigned long src_size, unsigned long ver_size, unsigned char type);
void DCBufferReset(CommandBuffer *buffer);

unsigned int DCB_get_next_gap(CommandBuffer *buff, unsigned long gap_req, 
    DCLoc *dc);
unsigned int DCB_commands_remain(CommandBuffer *buffer);
void DCB_get_next_command(CommandBuffer *buffer, DCommand *dc);
void DCB_truncate(CommandBuffer *buffer, unsigned long len);

void DCB_add_add(CommandBuffer *buffer, off_u64 ver_pos, unsigned long len,
    unsigned short src_id);
void DCB_add_copy(CommandBuffer *buffer, off_u64 src_pos, off_u64 ver_pos,
    unsigned long len);

void DCB_insert(CommandBuffer *buff);
void DCB_llm_init_buff(CommandBuffer *buff, unsigned long buff_size);
unsigned int DCB_test_llm_main(CommandBuffer *buff);
void DCB_test_total_copy_len(CommandBuffer *buff);

void DCB_resize_full(CommandBuffer *buffer);
void DCB_resize_matches(CommandBuffer *buffer);
void DCB_resize_llmatches(CommandBuffer *buffer);
#endif
