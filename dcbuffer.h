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

/* register_src flags */
#define DCB_FREE_SRC_CFH		(char)0x1
#define DCB_OVERLAY_SRC			(char)0x80

/* internal DCB src types */
#define DCB_CFH_SRC			(char)0x80
#define DCB_DCB_SRC			(char)0x00

/* essentially dcb->src_type is thus- upper 2 bits 
   the actual type of the src (eg, cfile, another command
   buffer), and the lower bit for if it's an add or copy src.
   The lower bit will likely be phased out, moving to a dynamic 
   add/copy determination depending on what the desired end result
   is.
*/

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
typedef struct _CommandBuffer *DCB_ptr;

typedef struct {
    DCLoc_match		data;
    DCB_ptr		src_dcb;
    unsigned long	cmd_pos;
    unsigned long	src_id;
    unsigned char	type;
} DCommand;

typedef unsigned long (*dcb_src_read_func)(DCommand *, unsigned long, 
    unsigned char *, unsigned long);
typedef unsigned long (*dcb_src_copy_func)(DCommand *, cfile *);

typedef union {
    cfile	*cfh;
    DCB_ptr	dcb;
} dcb_src;

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
	    unsigned long command_pos;
	    unsigned char *src_id;
	    DCLoc *lb_start, *lb_end, *lb_head, *lb_tail;
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
    dcb_src *srcs;
    dcb_src_read_func *src_read_func;
    dcb_src_copy_func *src_copy_func;
    unsigned int src_array_size, src_count;
    unsigned char src_type[256];
    unsigned char src_flags[256];

    /* this is a hack, and not a particularly good one either.
	things need to be expanded to eliminate the need for this- check the 
	bsdiff format reconstructor if you're curious of it's reason for 
	existing*/
    unsigned long flags;
} CommandBuffer;

#define copyDCB_add_src(dcb, dc, out_cfh)				\
    ((dc)->src_dcb->src_copy_func[(dc)->src_id]((dc), (out_cfh)))
#define copyDCB_copy_src(dcb, dc, out_cfh)				\
    ((dc)->src_dcb->src_copy_func[(dc)->src_id]((dc), (out_cfh)))


/* not used anymore, chuck at some point */
#define DCB_REGISTER_MATCHES_VER_CFH(buff, cfh)				\
    if((buff)->DCBtype==DCBUFFER_MATCHES_TYPE) {			\
	(buff)->DCB.matches.ver_start = cfile_start_offset((cfh));	\
    } else if((buff)->DCBtype==DCBUFFER_LLMATCHES_TYPE) {		\
	(buff)->DCB.llm.ver_start = cfile_start_offset((cfh));		\
    }

int internal_DCB_register_src(CommandBuffer *dcb, cfile *cfh,
    dcb_src_read_func read_func, dcb_src_copy_func copy_func, 
    unsigned char free, unsigned char type);
int DCB_register_overlay_srcs(CommandBuffer *dcb, 
    int *id1, cfile *src, dcb_src_read_func rf1, dcb_src_copy_func rc1, char free1,
    int *id2, cfile *add, dcb_src_read_func rf2, dcb_src_copy_func rc2, char free2);

#define DCB_REGISTER_ADD_SRC(dcb, cfh, func, free)	internal_DCB_register_src((dcb), (cfh), NULL, (func), DC_ADD, (free))
#define DCB_REGISTER_COPY_SRC(dcb, cfh, func, free)	internal_DCB_register_src((dcb), (cfh), NULL, (func), DC_COPY, (free))
#define DCB_register_src(dcb, cfh, rf, cf, free, type)  internal_DCB_register_src((dcb), (cfh), (rf), (cf), (type), (free))

unsigned long inline current_command_type(CommandBuffer *buff);

void DCBufferIncr(CommandBuffer *buffer);
void DCBufferDecr(CommandBuffer *buffer);
void DCBufferCollapseAdds(CommandBuffer *buffer);
void DCBufferFree(CommandBuffer *buffer);
int DCBufferInit(CommandBuffer *buffer, unsigned long buffer_size,
    unsigned long src_size, unsigned long ver_size, unsigned char type);
void DCBufferReset(CommandBuffer *buffer);

unsigned int DCB_get_next_gap(CommandBuffer *buff, unsigned long gap_req, 
    DCLoc *dc);
unsigned int DCB_commands_remain(CommandBuffer *buffer);
void internal_DCB_get_next_command(CommandBuffer *buffer, DCommand *dc);
void DCB_get_next_command(CommandBuffer *buffer, DCommand *dc);
#define DCB_get_next_actual_command(dcb, dc) \
	internal_DCB_get_next_command((dcb), (dc))

void DCB_truncate(CommandBuffer *buffer, unsigned long len);

void DCB_add_add(CommandBuffer *buffer, off_u64 ver_pos, unsigned long len,
    unsigned char src_id);
void DCB_add_copy(CommandBuffer *buffer, off_u64 src_pos, off_u64 ver_pos,
    unsigned long len, unsigned char src_id);

int DCB_insert(CommandBuffer *buff);
int DCB_llm_init_buff(CommandBuffer *buff, unsigned long buff_size);
unsigned int DCB_test_llm_main(CommandBuffer *buff);
void DCB_test_total_copy_len(CommandBuffer *buff);

int internal_DCB_resize_full(CommandBuffer *buffer);
int internal_DCB_resize_matches(CommandBuffer *buffer);
int internal_DCB_resize_llmatches(CommandBuffer *buffer);

int internal_DCB_resize_srcs(CommandBuffer *buffer);
#endif
