/*
  Copyright (C) 2003-2005 Brian Harring

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

#include "command_list.h"
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
#define DCBUFFER_BUFFERLESS_TYPE	0x8
#define DCBUFFER_BUFFERLESS_LINE_TYPE	0x10
#define DCBUFFER_LINE_TYPE		0x20

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
#define DCB_NULL_SRC			(char)0x20

typedef unsigned char DCB_SRC_ID;
//able to hold errors
typedef signed int   EDCB_SRC_ID;

// internal dcbuffer macros.
#define LLM_VEND(l)  ((l)->ver_pos + (l)->len)

typedef struct LL_DCLmatch LL_DCLmatch;

struct LL_DCLmatch {
    off_u64 src_pos, ver_pos;
    unsigned long len;
    LL_DCLmatch *next;
};

typedef struct _CommandBuffer *DCB_ptr;
typedef command_list overlay_chain;

typedef struct {
    DCLoc_match			data;
    struct _DCB_registered_src	*dcb_src;
    DCB_ptr			dcb_ptr;
    off_u64			ov_offset;
    off_u32			ov_len;
    unsigned char		type;
    DCB_SRC_ID			src_id;
} DCommand;

typedef struct {
    DCommand			*commands;
    unsigned int		size;
    unsigned int		count;
    unsigned int		pos;
    off_u32			len;
} DCommand_collapsed;

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
    overlay_chain	*ov;
    dcb_src_read_func	read_func;
    dcb_src_copy_func	copy_func;
    dcb_src_read_func   mask_read_func;
    unsigned char	flags;
} DCB_registered_src;


typedef void(*dcb_get_next_command)(struct _CommandBuffer *, DCommand *);
typedef void(*dcb_truncate_command)(struct _CommandBuffer *, unsigned long);
typedef void(*dcb_void_cb_command)(struct _CommandBuffer *);
typedef void(*dcb_void_dcb_command)(void *);
typedef int (*dcb_int_command)(void *);
typedef int (*dcb_add_overlay_command)(struct _CommandBuffer *, off_u64, off_u32, DCB_SRC_ID, off_u64, DCB_SRC_ID);
typedef int (*dcb_add_add_command)(struct _CommandBuffer *, off_u64, off_u32, DCB_SRC_ID);
typedef int (*dcb_add_copy_command)(struct _CommandBuffer *, off_u64, off_u64, off_u32, DCB_SRC_ID);

typedef struct _DCB_no_buff {
    DCommand		dc;
    cfile		*out_cfh;
} DCB_no_buff;

typedef struct _DCB_full {
    command_list 	cl;
    unsigned long 	command_pos;
} DCB_full;

typedef struct _DCB_matches {
    off_u64 ver_start;
    unsigned int buff_count, buff_size;
    DCLoc_match *buff, *cur;
    u_dcb_src		*gap_src;
} DCB_matches;

typedef struct _DCB_llm {
    off_u64 ver_start, gap_pos;
    LL_DCLmatch *main_head, *main;
    unsigned int buff_count, buff_size, main_count;
    LL_DCLmatch *buff, *cur;
    void **free;
    unsigned long free_size, free_count;
    unsigned char flags;
} DCB_llm;


typedef struct _CommandBuffer {
    off_u64 			src_size;
    off_u64 			ver_size;
    off_u64 			reconstruct_pos;
    unsigned char 		DCBtype;
    void 			*DCB;

    dcb_get_next_command	get_next;
    dcb_truncate_command	truncate;
    dcb_add_overlay_command	add_overlay;
    dcb_add_add_command		add_add;
    dcb_add_copy_command	add_copy;
    dcb_int_command		commands_remain;
    dcb_void_dcb_command	reset;
    dcb_void_dcb_command	free;
    dcb_int_command		finalize;
    dcb_void_cb_command		incr;
    dcb_void_cb_command		decr;

    DCB_registered_src		*srcs;
    unsigned short		src_count;
    unsigned short		src_array_size;
    unsigned long 		flags;
    DCB_registered_src		*default_add_src;
    DCB_registered_src		*default_copy_src;
#ifdef DEBUG_DCBUFFER
    off_u64 			total_copy_len;
#endif

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

int internal_DCB_register_volatile_cfh_src(CommandBuffer *dcb, cfile *cfh,
    dcb_src_read_func read_func, dcb_src_copy_func copy_func, 
    unsigned char free, unsigned char type);

#define DCB_REGISTER_VOLATILE_ADD_SRC(dcb, cfh, func, free)		\
    internal_DCB_register_volatile_cfh_src((dcb), (cfh), NULL, (func), DC_ADD, (free))
#define DCB_REGISTER_VOLATILE_COPY_SRC(dcb, cfh, func, free)		\
    internal_DCB_register_volatile_cfh_src((dcb), (cfh), NULL, (func), DC_COPY, (free))

#define DCB_REGISTER_ADD_SRC(dcb, cfh, func, free)	internal_DCB_register_cfh_src((dcb), (cfh), NULL, (func), DC_ADD, (free))
#define DCB_REGISTER_COPY_SRC(dcb, cfh, func, free)	internal_DCB_register_cfh_src((dcb), (cfh), NULL, (func), DC_COPY, (free))
#define DCB_register_src(dcb, cfh, rf, cf, free, type)  internal_DCB_register_cfh_src((dcb), (cfh), (rf), (cf), (type), (free))

unsigned long inline current_command_type(CommandBuffer *buff);

EDCB_SRC_ID DCB_register_fake_src(CommandBuffer *dcb, unsigned char type);
EDCB_SRC_ID DCB_dumb_clone_src(CommandBuffer *dcb, DCB_registered_src *drs, unsigned char type);
void DCB_register_out_cfh(CommandBuffer *dcb, cfile *out_cfh);
void DCB_free_commands(CommandBuffer *dcb);
DCBSearch * create_DCBSearch_index(CommandBuffer *dcb);
void free_DCBSearch_index(DCBSearch *s);
void tfree(void *p);

#define DCBufferIncr(buff)	\
(buff)->incr((buff))

#define DCBufferDecr(buff)	\
(buff)->decr((buff))

#define DCB_finalize(buff)	\
( ((buff)->finalize && (buff)->finalize((buff)->DCB)) || 0)

#define DCBufferCollapseAdds(buff)	\
(void *)NULL;

//(buffer)->collapse_adds != NULL && (buffer)->collapse_adds((buff)->DCB)

void DCBufferFree(CommandBuffer *buffer);
int DCB_common_init(CommandBuffer *, unsigned long, off_u64, off_u64, unsigned char);
int DCB_full_init(CommandBuffer *, unsigned long, off_u64, off_u64);
int DCB_matches_init(CommandBuffer *, unsigned long, off_u64, off_u64);
int DCB_llm_init(CommandBuffer *, unsigned long, off_u64, off_u64);
int DCB_no_buff_init(CommandBuffer *, unsigned long, off_u64, off_u64, cfile *);

#define DCBufferReset(buff)	\
(buff)->reconstruct_pos = 0;	\
if((buff)->reset != NULL) (buff)->reset((buff)->DCB);

#define DCB_insert DCB_finalize

unsigned int DCB_get_next_gap(CommandBuffer *buff, unsigned long gap_req, 
    DCLoc *dc);

//unsigned int DCB_commands_remain(CommandBuffer *buffer);

#define DCB_commands_remain(buff)	\
( ((buff)->commands_remain != NULL && (buff)->commands_remain((buff)->DCB)) || \
( (buff)->reconstruct_pos != (buff)->ver_size) )

#define DCB_truncate(buff, len) \
(buff)->truncate((buff), (len))

#define DCB_get_next_command(dcb, dc)	\
((dcb)->get_next((dcb), (dc)))


#define DCB_get_next_actual_command(dcb, dc) \
	DCB_get_next_command((dcb), (dc))

int DCB_add_overlay(CommandBuffer *buffer, off_u64 diff_src_pos, off_u32 len, 
  DCB_SRC_ID add_ov_id, off_u64 copy_src_pos, DCB_SRC_ID ov_src_id);

int DCB_rec_copy_from_DCB_src(CommandBuffer *tdcb, command_list *tcl,
    CommandBuffer *sdcb, command_list *scl, unsigned short *translation_map,
    unsigned long com_offset, off_u64 seek, off_u64 len);

#ifdef DEV_VERSION
int DCB_add_copy(CommandBuffer *buffer, off_u64 src_pos, off_u64 ver_pos, off_u32 len, DCB_SRC_ID src_id);
#else
#define DCB_add_copy(buff, sp, vp, l, si)  (buff)->add_copy((buff),(sp),(vp),(l),(si))
#endif

#ifdef DEV_VERSION
int DCB_add_add(CommandBuffer *buffer, off_u64 src_pos, off_u32 len, DCB_SRC_ID src_id);
#else
#define DCB_add_add(buff, sp, l, si) \
( ((buff)->add_add && (buff)->add_add((buff),(sp),(l),(si))) || 0)
#endif

off_u64
process_ovchain(CommandBuffer *dcb, off_u64 ver_pos, command_list *cl,
    overlay_chain *ov, unsigned long com_pos, unsigned long len);

command_list *
DCB_collapse_commands(CommandBuffer *dcb);

int DCB_llm_init_buff(CommandBuffer *buff, unsigned int buff_size);
unsigned int DCB_test_llm_main(CommandBuffer *buff);
void DCB_test_total_copy_len(CommandBuffer *buff);

unsigned long bail_if_called_func();

#define tfree(f) ((void *)f) != NULL && free((f))

#endif
