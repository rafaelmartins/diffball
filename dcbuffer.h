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

#define DCB_LLM_FINALIZED		0x2

/* register_src flags */
#define DCB_FREE_SRC_CFH		(char)0x1
#define DCB_OVERLAY_SRC			(char)0x80

/* dcb_src related */
#define DCB_SRC_NOT_TRANSLATED		0xffff


/* essentially the lower two bits of a register_src->type are add|copy;
   everything above is for specifying the actual type.
   expect the lower two bits to be phased out at some point, in favor of 
   either on the fly determining what the src_type is, or building a mask of 
   copy/add.
*/
/* internal DCB src types */
#define DCB_CFH_SRC			(char)0x80
#define DCB_DCB_SRC			(char)0x40

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
    // isn't it fun managing arrays, 'ey kiddies? :)
    unsigned char		*src_id;
    DCLoc			*command;
    unsigned long		com_count;
    unsigned long		com_size;
} command_list;

typedef command_list overlay_chain;

typedef struct {
    DCLoc_match			data;
    struct _DCB_registered_src	*dcb_src;
    DCB_ptr			dcb_ptr;
    unsigned long		ov_offset;
    unsigned long		ov_len;
    unsigned char		type;
    unsigned short		src_id;
} DCommand;

typedef struct {
    DCLoc_match			data;
    unsigned char		src_id;
} DCommand_abbrev;

typedef struct {
    unsigned long 	quanta;
    unsigned long 	index_size;
    unsigned long 	*index;
    off_u64		*ver_start;
} DCBSearch;	

typedef struct {
    unsigned short		src_map[256];
    DCB_ptr			src_dcb;
    DCBSearch			*s;
} DCB_src;

typedef union {
    cfile		*cfh;
    DCB_src		*dcb;
} u_dcb_src;

typedef unsigned long (*dcb_src_read_func)(u_dcb_src, unsigned long, 
    unsigned char *, unsigned long);
typedef unsigned long (*dcb_src_copy_func)(DCommand *, cfile *);

typedef struct _DCB_registered_src {
    u_dcb_src		src_ptr;
    unsigned char	type;
    overlay_chain	ov;
    dcb_src_read_func	read_func;
    dcb_src_copy_func	copy_func;
    dcb_src_read_func   mask_read_func;
    unsigned char	flags;
} DCB_registered_src;

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
	    command_list 	cl;
	    unsigned long 	command_pos;
/*	    unsigned long buffer_count;
	    unsigned long buffer_size;
	    unsigned long command_pos;
	    unsigned char *src_id;
	    DCLoc *lb_start, *lb_end, *lb_head, *lb_tail;
*/	} full;
	struct {
	    off_u64 ver_start;
	    unsigned long buff_count, buff_size;
	    DCLoc_match *buff, *cur;
	    u_dcb_src		*gap_src;
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

    DCB_registered_src	*srcs;
    unsigned short	src_count;
    unsigned short	src_array_size;
    unsigned long flags;
    DCB_registered_src	*default_add_src;
    DCB_registered_src	*default_copy_src;
} CommandBuffer;

#define copyDCB_add_src(dcb, dc, out_cfh)				\
    ((dc)->dcb_src->copy_func((dc), (out_cfh)))
#define copyDCB_copy_src(dcb, dc, out_cfh)				\
    ((dc)->dcb_src->copy_func((dc), (out_cfh)))


/* not used anymore, chuck at some point */
#define DCB_REGISTER_MATCHES_VER_CFH(buff, cfh)				\
    if((buff)->DCBtype==DCBUFFER_MATCHES_TYPE) {			\
	(buff)->DCB.matches.ver_start = cfile_start_offset((cfh));	\
    } else if((buff)->DCBtype==DCBUFFER_LLMATCHES_TYPE) {		\
	(buff)->DCB.llm.ver_start = cfile_start_offset((cfh));		\
    }

int DCB_register_dcb_src(CommandBuffer *dcb, CommandBuffer *dcb_src);
int DCB_register_overlay_src(CommandBuffer *dcb, 
    cfile *src, dcb_src_read_func rf1, dcb_src_copy_func rc1, 
    dcb_src_read_func rm1, char free1);

int internal_DCB_register_cfh_src(CommandBuffer *dcb, cfile *cfh,
    dcb_src_read_func read_func, dcb_src_copy_func copy_func, 
    unsigned char free, unsigned char type);

#define DCB_REGISTER_ADD_SRC(dcb, cfh, func, free)	internal_DCB_register_cfh_src((dcb), (cfh), NULL, (func), DC_ADD, (free))
#define DCB_REGISTER_COPY_SRC(dcb, cfh, func, free)	internal_DCB_register_cfh_src((dcb), (cfh), NULL, (func), DC_COPY, (free))
#define DCB_register_src(dcb, cfh, rf, cf, free, type)  internal_DCB_register_cfh_src((dcb), (cfh), (rf), (cf), (type), (free))

unsigned long inline current_command_type(CommandBuffer *buff);

DCBSearch * create_DCBSearch_index(CommandBuffer *dcb);
void free_DCBSearch_index(DCBSearch *s);
void tfree(void *p);

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

#define DCB_get_next_actual_command(dcb, dc) \
	internal_DCB_get_next_command((dcb), (dc))
#define DCB_get_next_command(dcb,dc) \
	internal_DCB_get_next_command((dcb), (dc))
	
void DCB_truncate(CommandBuffer *buffer, unsigned long len);

int DCB_add_overlay(CommandBuffer *buffer, off_u32 diff_src_pos, off_u32 len, 
    int add_ov_id, off_u32 copy_src_pos, int ov_src_id);
int DCB_add_add(CommandBuffer *buffer, off_u64 ver_pos, unsigned long len,
    unsigned char src_id);
int DCB_add_copy(CommandBuffer *buffer, off_u64 src_pos, off_u64 ver_pos,
    unsigned long len, unsigned char src_id);
int DCB_rec_copy_from_DCB_src(CommandBuffer *tdcb, command_list *tcl,
    CommandBuffer *sdcb, command_list *scl, unsigned short translation_map[256],
    unsigned long com_offset, off_u64 seek, off_u64 len);

off_u64
process_ovchain(CommandBuffer *dcb, off_u64 ver_pos, 
    DCommand_abbrev **dptr, DCommand_abbrev **odptr, overlay_chain *ov,
    unsigned long offset, unsigned long len);

int 
DCB_collapse_commands(CommandBuffer *dcb, DCommand_abbrev **dptr_p,
    unsigned long *len1, DCommand_abbrev **odptr_p, unsigned long *len2);

int DCB_insert(CommandBuffer *buff);
int DCB_llm_init_buff(CommandBuffer *buff, unsigned long buff_size);
unsigned int DCB_test_llm_main(CommandBuffer *buff);
void DCB_test_total_copy_len(CommandBuffer *buff);

int internal_DCB_resize_cl(command_list *cl);
int internal_DCB_resize_matches(CommandBuffer *buffer);
int internal_DCB_resize_llmatches(CommandBuffer *buffer);

int internal_DCB_resize_srcs(CommandBuffer *buffer);
unsigned long bail_if_called_func();
#endif
