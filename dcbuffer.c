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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "dcbuffer.h"
#include "command_list.h"
#include "cfile.h"
#include "bit-functions.h"
#include "defs.h"
#include "dcb-cfh-funcs.h"

extern unsigned int verbosity;

static int internal_DCB_llm_resize(DCB_llm *buff);
static int internal_DCB_llm_free_resize(DCB_llm *buff);
static int internal_DCB_matches_resize(DCB_matches *dcb);
static int internal_DCB_resize_cl(command_list *cl);
static inline int internal_DCB_resize_srcs(CommandBuffer *buffer);
int DCB_llm_finalize(void *);

unsigned long
bail_if_called_func()
{
	v0printf("err, wtf\n");
	abort();
}
		

void
DCB_free_commands(CommandBuffer *dcb)
{
	DCB_full *dcbf = (DCB_full *)dcb->DCB;
	unsigned long x = 0;
	if(dcbf->cl.com_count) {
		free(dcbf->cl.src_id);
		free(dcbf->cl.command);
		dcbf->cl.src_id = NULL;
		dcbf->cl.command = NULL;
		dcbf->cl.com_count = 0;
		dcbf->cl.com_size = 0;
	}
	for(x=0; x < dcb->src_count; x++) {
			if(dcb->srcs[x].ov) {
				free(dcb->srcs[x].ov->command);
				free(dcb->srcs[x].ov->src_id);
				free(dcb->srcs[x].ov);
				dcb->srcs[x].ov = NULL;
			}
			if(dcb->srcs[x].type & DCB_DCB_SRC) {
			if(dcb->srcs[x].src_ptr.dcb->s) {
				free_DCBSearch_index(dcb->srcs[x].src_ptr.dcb->s);
					dcb->srcs[x].src_ptr.dcb->s = NULL;
			}
		}
	}
}


signed int
init_DCommand_collapsed(DCommand_collapsed *dcc) {
	dcc->size = dcc->count = dcc->pos = 0;
	dcc->commands = (DCommand *)malloc(4 * sizeof(DCommand));
	if(dcc->commands == NULL) {
		return MEM_ERROR;
	}
	dcc->size = 4;
	return 0;
}


signed int
DCB_get_next_collapsed_command(CommandBuffer *dcb, DCommand_collapsed *dcc)
{
	if(dcc->count && dcc->pos) {
		dcc->commands[0] = dcc->commands[dcc->pos];
		if(! DCB_commands_remain(dcb)) {
			if(dcc->pos == dcc->count) {
				return dcc->pos = dcc->count = 0;
			}
			dcc->pos = dcc->count = 0;
			return 1;
		}
	} else {
		if(! DCB_commands_remain(dcb))
			 return 0;
		DCB_get_next_command(dcb, dcc->commands);
		while(DCB_commands_remain(dcb) && 0 == dcc->commands[0].data.len)
			DCB_get_next_command(dcb, dcc->commands);
		if(dcc->commands[0].data.len == 0) {
			return 0;
		}
	}
	dcc->count = 0;
	dcc->pos = 0;
	dcc->len = 0;
	do {
		dcc->len += dcc->commands[dcc->pos].data.len;
		dcc->pos++;
		if(! DCB_commands_remain(dcb)) {
			dcc->count = dcc->pos;
			return dcc->pos;
		}
		if(dcc->pos >= dcc->size) {
			// resize it.
			DCommand *dcc2 = realloc(dcc->commands, 2 * sizeof(DCommand) * dcc->size);
			if(dcc2 == NULL)
				return MEM_ERROR;
			dcc->commands = dcc2;
			dcc->size *= 2;
		}
		DCB_get_next_command(dcb, dcc->commands + dcc->pos);
		if(dcc->commands[dcc->pos].data.len == 0)
			dcc->pos--;
	} while (dcc->commands[dcc->pos].type == dcc->commands[dcc->pos -1].type);
	dcc->count = dcc->pos + 1;
	return dcc->pos;	
}


void
free_DCommand_collapsed(DCommand_collapsed *dcc)
{
	free(dcc->commands);
}


command_list *
DCB_collapse_commands(CommandBuffer *dcb)
{
	off_u64				ver_pos = 0;
	unsigned long		x = 0;
	command_list		 *cl;
	DCB_full				*dcbf = (DCB_full *)dcb->DCB;

	if((cl = (command_list *)malloc(sizeof(command_list) * dcb->src_count))==NULL)
			return NULL;
	for(x=0; x < dcb->src_count; x++) {
			if(CL_init(cl + x, 1, 0, 0)) {
			CL_free(cl);
			while(x > 1) {
					x--;
					CL_free(cl + x);
			}
			free(cl);
			return NULL;
		}
	}

	DCBufferReset(dcb);
	while(dcbf->cl.com_count > dcbf->command_pos) {

			// check if it's an overlay.
		if(dcb->srcs[dcbf->cl.src_id[dcbf->command_pos]].ov) {
			ver_pos = process_ovchain(dcb, ver_pos, cl,
				dcb->srcs[dcbf->cl.src_id[dcbf->command_pos]].ov,
				dcbf->cl.command[dcbf->command_pos].offset,
				dcbf->cl.command[dcbf->command_pos].len);
		} else {
			if(CL_add_full_command(cl + dcbf->cl.src_id[dcbf->command_pos],
					dcbf->cl.command[dcbf->command_pos].offset,
				dcbf->cl.command[dcbf->command_pos].len, ver_pos, 0)) {
				ver_pos = 0;
			} else {
					ver_pos += dcbf->cl.command[dcbf->command_pos].len;
			}
		}
		if(ver_pos == 0) {
			for(x=0; x < dcb->src_count; x++)
					CL_free(cl + x);
			CL_free(cl);
			return NULL;
		}
		DCBufferIncr(dcb);
	}
	DCBufferReset(dcb);
	return cl;
}

off_u64
process_ovchain(CommandBuffer *dcb, off_u64 ver_pos, 
	command_list *cl, overlay_chain *ov,
	unsigned long com_pos, unsigned long len)
{
	assert(len >= 2);
	assert(ov);
	assert(ov->com_size >= ov->com_count);
	assert(com_pos + len <= ov->com_count);

	//first command is *always* the mask command.
	if(CL_add_full_command(cl + ov->src_id[com_pos], ov->command[com_pos].offset, 
		ov->command[com_pos].len, ver_pos, ov->src_id[com_pos]))
			return 0;

	len--;
	com_pos++;
	while(len) {
			if(dcb->srcs[ov->src_id[com_pos]].ov) {

			/* tiz an overlay command.
			   no ov_chain should _EVER_ reference itself, hence the assert */
			assert(dcb->srcs[ov->src_id[com_pos]].ov != ov);

			ver_pos = process_ovchain(dcb, ver_pos, cl,
					dcb->srcs[ov->src_id[com_pos]].ov, 
					ov->command[com_pos].offset,
					ov->command[com_pos].len);
		} else {
			if(CL_add_full_command(cl + ov->src_id[com_pos], ov->command[com_pos].offset, 
					ov->command[com_pos].len, ver_pos, ov->src_id[com_pos]))
					ver_pos = 0;
			else
				ver_pos += ov->command[com_pos].len;
		}
		if(ver_pos == 0)
			return 0;
		len--;
		com_pos++;
	}
	return ver_pos;
}

EDCB_SRC_ID
DCB_register_overlay_src(CommandBuffer *dcb, 
	cfile *src, dcb_src_read_func rf1, dcb_src_copy_func rc1, 
	dcb_src_read_func rm1, char free1)
{
	EDCB_SRC_ID id;
	id = internal_DCB_register_cfh_src(dcb, src, rf1, rc1, DC_ADD, (free1 | DCB_OVERLAY_SRC));
	if(id < 0) {
			return id;
	}
	if((dcb->srcs[id].ov = (overlay_chain *)malloc(sizeof(overlay_chain)))==NULL)
			return MEM_ERROR;

	if(rm1 != NULL)
		dcb->srcs[id].mask_read_func = rm1;
	else
			dcb->srcs[id].mask_read_func = default_dcb_src_cfh_read_func;

	if(CL_init(dcb->srcs[id].ov, 0, 0, 1))
			return MEM_ERROR;
	return id;
}

int
DCB_register_dcb_src(CommandBuffer *dcb, CommandBuffer *dcb_src)
{
	unsigned short x;
	assert(dcb->DCBtype == DCBUFFER_FULL_TYPE);
	assert(dcb_src->DCBtype == DCBUFFER_FULL_TYPE);
	if((dcb->srcs[dcb->src_count].src_ptr.dcb = (DCB_src *)malloc(sizeof(DCB_src))) == NULL) {
			return MEM_ERROR;
	}
	for(x=0; x < 256; x++)
			dcb->srcs[dcb->src_count].src_ptr.dcb->src_map[x] = DCB_SRC_NOT_TRANSLATED;
			
	dcb->srcs[dcb->src_count].src_ptr.dcb->src_dcb = dcb_src;
	dcb->srcs[dcb->src_count].src_ptr.dcb->s = create_DCBSearch_index(dcb_src);

	if(dcb->srcs[dcb->src_count].src_ptr.dcb->s == NULL) {
			free(dcb->srcs[dcb->src_count].src_ptr.dcb);
			return MEM_ERROR;
	}

	dcb->srcs[dcb->src_count].flags = 0;
	dcb->srcs[dcb->src_count].read_func = NULL;
	dcb->srcs[dcb->src_count].mask_read_func = NULL;
	dcb->srcs[dcb->src_count].copy_func = NULL;
	dcb->srcs[dcb->src_count].ov = NULL;
	dcb->srcs[dcb->src_count].type = ((DC_COPY & 0x1) | DCB_DCB_SRC);
	dcb->src_count++;
	return dcb->src_count - 1;
}

EDCB_SRC_ID
DCB_register_fake_src(CommandBuffer *dcb, unsigned char type)
{
	dcb->srcs[dcb->src_count].src_ptr.cfh = NULL;
	dcb->srcs[dcb->src_count].flags = 0;
	dcb->srcs[dcb->src_count].type = ((type & 0x1) | DCB_NULL_SRC);
	dcb->srcs[dcb->src_count].read_func = &bail_if_called_func;
	dcb->srcs[dcb->src_count].copy_func = &bail_if_called_func;
	dcb->srcs[dcb->src_count].mask_read_func = NULL;
	dcb->srcs[dcb->src_count].ov = NULL;
	dcb->src_count++;
	return dcb->src_count - 1;
}

EDCB_SRC_ID
DCB_dumb_clone_src(CommandBuffer *dcb, DCB_registered_src *drs, unsigned char type)
{
	v3printf("registering cloned src as buffer id(%u)\n", dcb->src_count);

	if(dcb->src_count == dcb->src_array_size && internal_DCB_resize_srcs(dcb)) {
		return MEM_ERROR;
	}

	dcb->srcs[dcb->src_count].src_ptr = drs->src_ptr;
	dcb->srcs[dcb->src_count].flags = drs->flags;
	dcb->srcs[dcb->src_count].type = type;
	dcb->srcs[dcb->src_count].read_func = drs->read_func;
	dcb->srcs[dcb->src_count].copy_func = drs->copy_func;
	dcb->srcs[dcb->src_count].mask_read_func = drs->mask_read_func;
	dcb->srcs[dcb->src_count].ov = drs->ov;

	if(DCBUFFER_FULL_TYPE != dcb->DCBtype) {
		if(type & DC_COPY) {
			dcb->default_copy_src = dcb->srcs + dcb->src_count;
		} else {
			dcb->default_add_src = dcb->srcs + dcb->src_count;
		}
	}
	dcb->src_count++;
	return dcb->src_count - 1;
}

EDCB_SRC_ID
internal_DCB_register_volatile_cfh_src(CommandBuffer *dcb, cfile *cfh, 
	dcb_src_read_func read_func, 
	dcb_src_copy_func copy_func,
	unsigned char type, unsigned char flags)
{
	cfile *dup;
	EDCB_SRC_ID x;

	if(DCBUFFER_BUFFERLESS_TYPE == dcb->DCBtype) {
		v2printf("registering volatile handle, free(%u)\n", flags);
		dup = copen_dup_cfh(cfh);
		if(dup == NULL)
			return MEM_ERROR;
		if(dup == NULL)
			return MEM_ERROR;
		x = internal_DCB_register_cfh_src(dcb, dup, read_func, copy_func, type, (flags | DCB_FREE_SRC_CFH));
		if(x < 0) {
			cclose(dup);
			return MEM_ERROR;
		}
		if(flags & DCB_FREE_SRC_CFH) {
			if(0 > internal_DCB_register_cfh_src(dcb,cfh,read_func,copy_func,type,flags)) {
					return MEM_ERROR;
			}
		}
		return x;
	} else {
			return internal_DCB_register_cfh_src(dcb,cfh,read_func,copy_func,type,flags);
	}
}
			

EDCB_SRC_ID
internal_DCB_register_cfh_src(CommandBuffer *dcb, cfile *cfh, 
	dcb_src_read_func read_func, 
	dcb_src_copy_func copy_func,
	unsigned char type, unsigned char flags)
{
	v3printf("registering cfh_id(%u), as buffer id(%u)\n", cfh->cfh_id, dcb->src_count);

	if(dcb->src_count == dcb->src_array_size && internal_DCB_resize_srcs(dcb)) {
		return MEM_ERROR;
	}

	v2printf("registering %u src\n", dcb->src_count);
	dcb->srcs[dcb->src_count].src_ptr.cfh = cfh;
	dcb->srcs[dcb->src_count].flags = flags;
	dcb->srcs[dcb->src_count].type = ((type & 0x1) | DCB_CFH_SRC);
	dcb->srcs[dcb->src_count].read_func = (read_func == NULL ? default_dcb_src_cfh_read_func : read_func);
	dcb->srcs[dcb->src_count].copy_func = (copy_func == NULL ? default_dcb_src_cfh_copy_func : copy_func);
	dcb->srcs[dcb->src_count].mask_read_func = NULL;
	dcb->srcs[dcb->src_count].ov = NULL;

	if(DCBUFFER_FULL_TYPE != dcb->DCBtype) {
		if(type & DC_COPY) {
			dcb->default_copy_src = dcb->srcs + dcb->src_count;
		} else {
			dcb->default_add_src = dcb->srcs + dcb->src_count;
		}
	}

	dcb->src_count++;
	return dcb->src_count - 1;
}

static inline int
internal_DCB_resize_srcs(CommandBuffer *dcb)
{
	if((dcb->srcs = (DCB_registered_src *)realloc(dcb->srcs, sizeof(DCB_registered_src) 
		* dcb->src_array_size * 2))==NULL) {
		return MEM_ERROR;
	}

	dcb->src_array_size *= 2;
	return 0;
}


void
DCB_test_total_copy_len(CommandBuffer *buff)
{
#ifdef DEBUG_DCBUFFER
	unsigned long computed_len = 0;
	DCommand dc;
	DCBufferReset(buff);

	while(DCB_commands_remain(buff)) {
		DCB_get_next_command(buff, &dc);
		if(dc.type==DC_COPY)
			computed_len += dc.data.len;
	}

	v0printf("dcbuffer test: copy_len(%lu==%lu)\n", buff->total_copy_len,
		computed_len);
	DCBufferReset(buff);
#endif
}

// collapse this into a loop for adds.
unsigned int
DCB_get_next_gap(CommandBuffer *buff, unsigned long gap_req, DCLoc *dc)
{
	DCB_llm *dcb = (DCB_llm *)buff->DCB;
	assert(dcb->flags & DCB_LLM_FINALIZED);
	dc->len = dc->offset = 0;

	if(dcb->gap_pos == buff->ver_size){ 
		return 0;
	} else if(dcb->gap_pos == 0) {
		if(dcb->main_count==0) {
			if(gap_req <= buff->ver_size) {
				dc->len = buff->ver_size;
			}
			dcb->gap_pos = buff->ver_size;
			return dc->len != 0;
		} else if(dcb->main_head->ver_pos >= gap_req) {
			dcb->gap_pos = dc->len = dcb->main_head->ver_pos;
			return 1;
		}
	}
	while(dcb->main != NULL  && dcb->gap_pos > dcb->main->ver_pos) {
			DCBufferIncr(buff);
	}
	while(dcb->main != NULL && dc->len == 0) {
		if(dcb->main->next == NULL) {
			dcb->gap_pos = buff->ver_size;
			if(buff->ver_size - LLM_VEND(dcb->main) >= gap_req) {
				dc->offset = LLM_VEND(dcb->main);
				dc->len = buff->ver_size - LLM_VEND(dcb->main);
			} else {
				DCBufferIncr(buff);
				return 0;
			}
		} else if(dcb->main->next->ver_pos - LLM_VEND(dcb->main) >= gap_req) {
			dc->offset = LLM_VEND(dcb->main);
			dc->len = dcb->main->next->ver_pos - dc->offset;
			dcb->gap_pos = dcb->main->next->ver_pos;
		} else {
			DCBufferIncr(buff);
		}
	}
	if(dc->len != 0) {
		return 1;
	}
	return 0;
}

void
DCB_full_get_next(CommandBuffer *buff, DCommand *dc)
{
	DCB_full *dcb = (DCB_full *)buff->DCB;
	dc->type = (buff->srcs[dcb->cl.src_id[dcb->command_pos]].type & 0x1);
	dc->data.ver_pos = buff->reconstruct_pos;
	dc->dcb_src = buff->srcs + dcb->cl.src_id[dcb->command_pos];
	if(dc->dcb_src->ov) {
		dc->ov_offset = dcb->cl.command[dcb->command_pos].offset;
		dc->ov_len = dcb->cl.command[dcb->command_pos].len;
		dc->data.src_pos = dc->dcb_src->ov->command[dc->ov_offset].offset;
		dc->data.len = dc->dcb_src->ov->command[dc->ov_offset].len;
	} else {
		dc->data.src_pos = dcb->cl.command[dcb->command_pos].offset;
		dc->data.len = dcb->cl.command[dcb->command_pos].len;
		dc->ov_offset = 0;
		dc->ov_len = 0;
	}
	DCBufferIncr(buff);

	dc->dcb_ptr = buff;
	dc->src_id = dc->dcb_src - buff->srcs;
	buff->reconstruct_pos += dc->data.len;
}

void
DCB_matches_get_next(CommandBuffer *buff, DCommand *dc)
{
	DCB_matches *dcb = (DCB_matches *)buff->DCB;
	assert(buff->reconstruct_pos != buff->ver_size);
	if((dcb->cur - dcb->buff) == dcb->buff_count) {
		dc->type = DC_ADD;
		dc->dcb_src = buff->default_add_src;
		dc->data.src_pos = dc->data.ver_pos = buff->reconstruct_pos;
		dc->data.len = buff->ver_size - buff->reconstruct_pos;
	} else if(buff->reconstruct_pos == dcb->cur->ver_pos) {
		dc->type = DC_COPY;
		dc->dcb_src = buff->default_copy_src;
		dc->data.src_pos = dcb->cur->src_pos;
		dc->data.ver_pos = dcb->cur->ver_pos;
		dc->data.len = dcb->cur->len;
		DCBufferIncr(buff);
	} else {
		dc->type = DC_ADD;
		dc->dcb_src = buff->default_add_src;
		dc->data.src_pos = dc->data.ver_pos = buff->reconstruct_pos;
		dc->data.len = dcb->cur->ver_pos - buff->reconstruct_pos;
	}
	dc->ov_offset = 0;
	dc->ov_len = 0;

	dc->dcb_ptr = buff;
	dc->src_id = dc->dcb_src - buff->srcs;
	buff->reconstruct_pos += dc->data.len;
}

void
DCB_llm_get_next(CommandBuffer *buff, DCommand *dc)
{
	DCB_llm *dcb = (DCB_llm *)buff->DCB;
	assert(dcb->flags & DCB_LLM_FINALIZED);
	if(dcb->main == NULL) {
		dc->type = DC_ADD;
		dc->dcb_src = buff->default_add_src;
		dc->data.src_pos = buff->reconstruct_pos;
		dc->data.ver_pos = buff->reconstruct_pos;
		dc->data.len = buff->ver_size - buff->reconstruct_pos;
	} else if(buff->reconstruct_pos == dcb->main->ver_pos) {
		dc->type = DC_COPY;
		dc->dcb_src = buff->default_copy_src;
		dc->data.src_pos = dcb->main->src_pos;
		dc->data.ver_pos = buff->reconstruct_pos;
		dc->data.len = dcb->main->len;
		DCBufferIncr(buff);
	} else {
		dc->type = DC_ADD;
		dc->dcb_src = buff->default_add_src;
		dc->data.src_pos = buff->reconstruct_pos;
		dc->data.ver_pos = buff->reconstruct_pos;
		dc->data.len = dcb->main->ver_pos - buff->reconstruct_pos;
	}
	dc->ov_offset = 0;
	dc->ov_len = 0;

	dc->dcb_ptr = buff;
	dc->src_id = dc->dcb_src - buff->srcs;
	buff->reconstruct_pos += dc->data.len;
}


void
DCB_full_truncate(CommandBuffer *buffer, unsigned long len)
{
	DCB_full *dcb = (DCB_full *)buffer->DCB;
	/* get the tail to an actual node. */
	DCBufferDecr(buffer);
	buffer->reconstruct_pos -= len;
	while(len) {
		if (dcb->cl.command[dcb->command_pos].len <= len) {

#ifdef DEBUG_DCBUFFER
			if((buff->srcs[dcb->cl.src_id[dcb->command_pos]].type & 0x1) == DC_COPY) {
				buffer->total_copy_len -= dcb->cl.command[dcb->command_pos].len;
			}
#endif

			len -= dcb->cl.command[dcb->command_pos].len;
			DCBufferDecr(buffer);
			dcb->cl.com_count--;
		} else {

#ifdef DEBUG_DCBUFFER
			buffer->total_copy_len -= len;
#endif
			dcb->cl.command[dcb->command_pos].len -= len;
			len=0;
		}
	}
	DCBufferIncr(buffer);
}

void
DCB_matches_truncate(CommandBuffer *buffer, unsigned long len)
{
	off_u64 trunc_pos;
	DCB_matches *dcb = (DCB_matches *)buffer->DCB;
	DCBufferDecr(buffer);
	assert(dcb->buff_count > 0);
	assert(LLM_VEND(dcb->cur) - len >= 0);
	trunc_pos = LLM_VEND(dcb->cur) - len;
	while(dcb->buff_count > 0 && trunc_pos < LLM_VEND(dcb->cur)) {
		if(dcb->cur->ver_pos >= trunc_pos) {
#ifdef DEBUG_DCBUFFER
			buffer->total_copy_len -= dcb->cur->len;
#endif
			DCBufferDecr(buffer);
			dcb->buff_count--;
		} else {
#ifdef DEBUG_DCBUFFER
			buffer->total_copy_len -= trunc_pos - dcb->cur->ver_pos;
#endif
			dcb->cur->len = trunc_pos - dcb->cur->ver_pos;
		}
   }
   if(dcb->buff_count == 0 ) {
		buffer->reconstruct_pos = dcb->ver_start;
   } else { 
		buffer->reconstruct_pos = LLM_VEND(dcb->cur);
		DCBufferIncr(buffer);
   }
}

void
DCB_llm_truncate(CommandBuffer *buffer, unsigned long len)
{
	off_u64 trunc_pos;
	DCB_llm *dcb = (DCB_llm *)buffer->DCB;
	DCBufferDecr(buffer);
	assert(dcb->buff_count > 0);
	assert(LLM_VEND(dcb->cur) - len >= 0);
	trunc_pos = LLM_VEND(dcb->cur) - len;
	while(dcb->buff_count > 0 && trunc_pos < LLM_VEND(dcb->cur)) {
		if(dcb->cur->ver_pos >= trunc_pos) {

#ifdef DEBUG_DCBUFFER
			buffer->total_copy_len -= dcb->cur->len;
#endif
			dcb->buff_count--;
			DCBufferDecr(buffer);

		} else {

#ifdef DEBUG_DCBUFFER
			buffer->total_copy_len -= LLM_VEND(dcb->cur) - trunc_pos;
#endif
			dcb->cur->len = trunc_pos - dcb->cur->ver_pos;
		}
	}
	if(dcb->buff_count == 0 ) {
		buffer->reconstruct_pos = dcb->ver_start;
	} else {
		buffer->reconstruct_pos = LLM_VEND(dcb->cur);
		DCBufferIncr(buffer);
	}
	assert(dcb->buff_count == (dcb->cur - dcb->buff));
}

void
DCB_full_incr(CommandBuffer *buff)
{
	((DCB_full *)buff->DCB)->command_pos++;
}

void
DCB_matches_incr(CommandBuffer *buff)
{
	DCB_matches *dcb = (DCB_matches *)buff->DCB;
	assert(dcb->cur - dcb->buff < dcb->buff_size);
	dcb->cur++;
}
void
DCB_llm_incr(CommandBuffer *buff)
{
	DCB_llm *dcb = (DCB_llm *)buff->DCB;
	if(dcb->flags & DCB_LLM_FINALIZED) {
		assert(dcb->main != NULL);
		dcb->main = dcb->main->next;
	} else {
		assert(dcb->cur - dcb->buff < dcb->buff_size);
		dcb->cur++;
	}
}

void
DCB_full_decr(CommandBuffer *buff)
{
	((DCB_full *)buff->DCB)->command_pos--;
}

void
DCB_matches_decr(CommandBuffer *buff)
{
	DCB_matches *dcb = (DCB_matches *)buff->DCB;
	if(dcb->cur != dcb->buff) {
		dcb->cur--;
	} else {
		buff->reconstruct_pos = 0;
	}
}

void
DCB_llm_decr(CommandBuffer *buff)
{
	DCB_llm *dcb = (DCB_llm *)buff->DCB;
	assert((DCB_LLM_FINALIZED & dcb->flags)==0);
	if(dcb->cur != dcb->buff) {
		assert(dcb->cur != 0);
		dcb->cur--;
	} else {
		buff->reconstruct_pos = 0;
	}
}

// at the moment, this is designed to basically give the finger when it detects a DCB_SRC_DCB, w/in sdcb.
// I don't want to get into recursively registering DCB's as srcs through versions- this may (likely will)
// change down the line, once this code has been stabled, and I feel the need/urge.
//
// meanwhile, suffer the assert :)

int
DCB_rec_copy_from_DCB_src(CommandBuffer *tdcb, command_list *tcl,
	CommandBuffer *sdcb, command_list *scl, unsigned short *translation_map,
	unsigned long com_offset, off_u64 seek, off_u64 len)
{
	unsigned long index;
	off_u64 tmp_len;
	signed short int x;
	unsigned long size_check;
	DCLoc *cur;
	DCB_registered_src *dcb_s;

	assert(sdcb == tdcb || translation_map != NULL);
	assert(com_offset < scl->com_count);
	assert(tcl != scl);
	
	// adjust position appropriately
	//   this likely is broke for an initial overlay.
	if(sdcb->srcs[scl->src_id[com_offset]].ov && sdcb->srcs[scl->src_id[com_offset]].ov != scl)
			tmp_len = sdcb->srcs[scl->src_id[com_offset]].ov->command[scl->command[com_offset].offset].len;
	else
			tmp_len = scl->command[com_offset].len;
	while(seek >= tmp_len) {
		seek -= tmp_len;
			com_offset++;
		assert(com_offset < scl->com_count);

			if(seek) {
				if(sdcb->srcs[scl->src_id[com_offset]].ov)
						tmp_len = sdcb->srcs[scl->src_id[com_offset]].ov->command[scl->command[com_offset].offset].len;
				else
						tmp_len = scl->command[com_offset].len;
			}
			   
	}

	while(len) {
		cur = scl->command + com_offset;
		dcb_s = sdcb->srcs + scl->src_id[com_offset];

		tmp_len = MIN(cur->len - seek, len);

			if(dcb_s->type & DCB_DCB_SRC) {

			// only allow translating one dcb version; other wise would have to recursively 
			//   update maps to tdcb

			index = cur->offset / dcb_s->src_ptr.dcb->s->quanta;

			assert(sdcb == tdcb);
			assert(index < dcb_s->src_ptr.dcb->s->index_size);
			assert(dcb_s->src_ptr.dcb->s->ver_start[index] <= cur->offset);

			assert(tcl != &((DCB_full *)dcb_s->src_ptr.dcb->src_dcb->DCB)->cl);
			if(DCB_rec_copy_from_DCB_src(tdcb, 
					tcl, 
					dcb_s->src_ptr.dcb->src_dcb,
					&((DCB_full *)dcb_s->src_ptr.dcb->src_dcb->DCB)->cl,
					dcb_s->src_ptr.dcb->src_map,
				dcb_s->src_ptr.dcb->s->index[index],
				cur->offset - dcb_s->src_ptr.dcb->s->ver_start[index] + seek,
				tmp_len)) {
			}
		} else {
			if(sdcb != tdcb) {

					// we're not working on the same version, so the map must be used, and updated.
					if(translation_map[scl->src_id[com_offset]] == DCB_SRC_NOT_TRANSLATED) {
					v2printf("registering %u translated ", scl->src_id[com_offset]);
					if(dcb_s->ov) {
								x = DCB_register_overlay_src(tdcb, dcb_s->src_ptr.cfh,
									dcb_s->read_func, dcb_s->copy_func, dcb_s->mask_read_func,
									(dcb_s->flags & DCB_FREE_SRC_CFH));
						} else {
//						x = DCB_register_src(tdcb, dcb_s->src_ptr.cfh, 
//									dcb_s->read_func, dcb_s->copy_func, 
//									dcb_s->flags, (dcb_s->type & 0x1));

						x = DCB_dumb_clone_src(tdcb, dcb_s, (dcb_s->type & DC_COPY));
						}
						if(x < 0)
							return x;
					v2printf("as %u\n", x);

					translation_map[scl->src_id[com_offset]] = x;

					// disable auto-freeing in the parent; leave the flag on tdcb's version of src
						dcb_s->flags &= ~DCB_FREE_SRC_CFH;
					} else {
					x = translation_map[scl->src_id[com_offset]];
					assert(!(tdcb->srcs[x].type & DCB_DCB_SRC));
					}
			} else {
					x = scl->src_id[com_offset];
			}
			if(sdcb->srcs[scl->src_id[com_offset]].ov && scl != sdcb->srcs[scl->src_id[com_offset]].ov) {
					// this is an ov, and we're not processing the first masking command.
					// so, we go recursive 
					// note temp_len is inaccurate when encountering the command to jump from ov_chain 
					// to another
				assert(x < tdcb->src_count);
				assert(tcl != tdcb->srcs[x].ov);
					tmp_len = MIN(len, sdcb->srcs[scl->src_id[com_offset]].ov->command[cur->offset].len - seek);
					index = tdcb->srcs[x].ov->com_count;
					if(tdcb->srcs[x].ov->com_count + 2 >= tdcb->srcs[x].ov->com_size) {
						if(internal_DCB_resize_cl(tdcb->srcs[x].ov))
								return MEM_ERROR;
					}
					tdcb->srcs[x].ov->src_id[index] = x;
					tdcb->srcs[x].ov->command[index].offset = sdcb->srcs[scl->src_id[com_offset]].ov->command[cur->offset].offset + seek;
					tdcb->srcs[x].ov->command[index].len = tmp_len;
					tdcb->srcs[x].ov->com_count++;
				size_check = tcl->com_count;
				assert(tdcb->srcs[x].ov != sdcb->srcs[scl->src_id[com_offset]].ov);
					if(DCB_rec_copy_from_DCB_src(tdcb, tdcb->srcs[x].ov, sdcb,
						sdcb->srcs[scl->src_id[com_offset]].ov,
						translation_map,
						scl->command[com_offset].offset + 1,
						seek,
						tmp_len)){
						return MEM_ERROR;
					}
					
				assert(size_check == tcl->com_count);
				if(tcl->com_count == tcl->com_size) {
					if(internal_DCB_resize_cl(tcl))
							return MEM_ERROR;
				}
					tcl->command[tcl->com_count].offset = index;
					tcl->command[tcl->com_count].len = tdcb->srcs[x].ov->com_count - index;
					tcl->src_id[tcl->com_count] = x;
			} else {
				if(tcl->com_count == tcl->com_size) {
					if(internal_DCB_resize_cl(tcl))
						return MEM_ERROR;
				}
			
					tcl->src_id[tcl->com_count] = x;
				tcl->command[tcl->com_count].offset = cur->offset + seek;
					tcl->command[tcl->com_count].len = tmp_len;
			}
			tcl->com_count++;
		}
		len -= tmp_len;
		seek = 0;
		com_offset++;

		// if at the end of scl, we best be at the end of this traversal.
		assert(com_offset < scl->com_count || len == 0);
	}
	return 0;
}

int
DCB_add_overlay(CommandBuffer *dcb, off_u64 diff_src_pos, off_u32 len, DCB_SRC_ID add_ov_id,
	off_u64 copy_src_pos, DCB_SRC_ID ov_src_id)
{
	// error and sanity checks needed.
	command_list *ov;
	unsigned long index;
	unsigned long orig_com_count;
	
	// best have an overlay command_list, otherwise they're trying an to add an overlay 
	// command to a non-overlay registered_src.
	assert(dcb->srcs[add_ov_id].ov);
	ov = dcb->srcs[add_ov_id].ov;
	assert(ov->com_size);

	if(DCBUFFER_BUFFERLESS_TYPE == dcb->DCBtype) {
			ov->com_count = 0;
			assert((dcb->srcs[ov_src_id].type & DCB_DCB_SRC) == 0);
	}
	
	orig_com_count = ov->com_count;
	if(CL_add_command(ov, diff_src_pos, len, add_ov_id))
			return MEM_ERROR;
	if(dcb->srcs[ov_src_id].type & DCB_DCB_SRC) {
			index = copy_src_pos / dcb->srcs[ov_src_id].src_ptr.dcb->s->quanta;

		// checks on the DCBSearch
			assert(index < dcb->srcs[ov_src_id].src_ptr.dcb->s->index_size);
			assert(copy_src_pos + len >= dcb->srcs[ov_src_id].src_ptr.dcb->s->ver_start[index]);

			if(DCB_rec_copy_from_DCB_src(dcb, ov,
				dcb->srcs[ov_src_id].src_ptr.dcb->src_dcb,
				&((DCB_full *)dcb->srcs[ov_src_id].src_ptr.dcb->src_dcb->DCB)->cl,
				dcb->srcs[ov_src_id].src_ptr.dcb->src_map,
				dcb->srcs[ov_src_id].src_ptr.dcb->s->index[index],
				copy_src_pos - dcb->srcs[ov_src_id].src_ptr.dcb->s->ver_start[index],
				len)) {
				return MEM_ERROR;
			}
	} else {
		if(CL_add_command(ov, copy_src_pos, len, ov_src_id))
			return MEM_ERROR;
	}
	
	// remember, ov_commands in the chain are prefixed w/ the full mask offset/len, then the relevant commands
	// to be used for src.  so... ov additions *must* always be at least 2 commands.
	assert(ov->com_count - orig_com_count > 1);
	
	if(DCBUFFER_BUFFERLESS_TYPE == dcb->DCBtype) {
			DCB_no_buff *dcbnb = (DCB_no_buff *)dcb->DCB;
			dcbnb->dc.type = (dcb->srcs[add_ov_id].type & 0x1);
			dcbnb->dc.data.src_pos = diff_src_pos;
			dcbnb->dc.data.ver_pos = dcb->reconstruct_pos;
			dcbnb->dc.data.len = len;
			dcbnb->dc.dcb_ptr = dcb;
			dcbnb->dc.dcb_src = dcb->srcs + add_ov_id;
			dcbnb->dc.ov_len = 2;
			dcbnb->dc.ov_offset = 0;
			if(len != copyDCB_add_src(dcb, &dcbnb->dc, dcbnb->out_cfh)) {
				v1printf("error executing add_overlay during bufferless mode\n")
				return IO_ERROR;
			}
			dcb->reconstruct_pos += len;
	} else {
			DCB_add_add(dcb, orig_com_count, ov->com_count - orig_com_count, add_ov_id);
			dcb->reconstruct_pos += len - (ov->com_count - orig_com_count);
	}

	return 0L;
}

#ifdef DEV_VERSION
int 
DCB_add_add(CommandBuffer *buffer, off_u64 src_pos, off_u32 len, DCB_SRC_ID src_id)
{
	v3printf("add v(%llu), l(%u), id(%u), rpos(%llu)\n", (act_off_u64)src_pos, len, src_id, 
		(act_off_u64)buffer->reconstruct_pos);
	if(buffer->add_add)
		return buffer->add_add(buffer, src_pos, len, src_id);
	return 0;
}
#endif

int
DCB_no_buff_add_add(CommandBuffer *buffer, off_u64 src_pos, off_u32 len, DCB_SRC_ID src_id)
{
	DCB_no_buff *dcb = (DCB_no_buff *)buffer->DCB;
	dcb->dc.type = (buffer->srcs[src_id].type & 0x1);
	dcb->dc.data.src_pos = src_pos;
	dcb->dc.data.len = len;
	dcb->dc.data.ver_pos = buffer->reconstruct_pos;
	dcb->dc.src_id = src_id;
	dcb->dc.dcb_ptr = buffer;
	dcb->dc.dcb_src = buffer->srcs + src_id;
	if(len != copyDCB_add_src(buffer, &dcb->dc, dcb->out_cfh)) {
			v1printf("error executing add_add during bufferless mode\n");
			return IO_ERROR;
	}
	buffer->reconstruct_pos += len;
	return 0;
}

int
DCB_full_add_add(CommandBuffer *buffer, off_u64 src_pos, off_u32 len, DCB_SRC_ID src_id)
{
	DCB_full *dcb = (DCB_full *)buffer->DCB;

	if(dcb->cl.com_count == dcb->cl.com_size &&		internal_DCB_resize_cl(&dcb->cl))
		return MEM_ERROR;

	dcb->cl.src_id[dcb->command_pos] = src_id;
	dcb->cl.command[dcb->command_pos].offset = src_pos;
	dcb->cl.command[dcb->command_pos].len = len;
	dcb->cl.com_count++;
	buffer->reconstruct_pos += len;
	DCBufferIncr(buffer);
	return 0;
}


#ifdef DEV_VERSION
int
DCB_add_copy(CommandBuffer *buffer, off_u64 src_pos, off_u64 ver_pos, off_u32 len, DCB_SRC_ID src_id)
{
#ifdef DEBUG_DCBUFFER
	buffer->total_copy_len += len;
#endif

	v3printf("copy s(%llu), v(%llu), l(%u), rpos(%llu)\n", (act_off_u64)src_pos, (act_off_u64)ver_pos ,
		 len, (act_off_u64)buffer->reconstruct_pos);
	return buffer->add_copy(buffer, src_pos, ver_pos, len, src_id);
}
#endif

int
DCB_no_buff_add_copy(CommandBuffer *buffer, off_u64 src_pos, off_u64 ver_pos, off_u32 len, DCB_SRC_ID src_id)
{
	DCB_no_buff *dcb = (DCB_no_buff *)buffer->DCB;
	dcb->dc.type = (buffer->srcs[src_id].type & 0x1);
	dcb->dc.data.src_pos = src_pos;
	dcb->dc.data.ver_pos = ver_pos;
	dcb->dc.data.len = len;
	dcb->dc.src_id = src_id;
	dcb->dc.dcb_ptr = buffer;
	dcb->dc.dcb_src = buffer->srcs + src_id;
	if(len != copyDCB_add_src(buffer, &dcb->dc, dcb->out_cfh)) {
			v1printf("error executing add_copy during bufferless mode\n");
			return IO_ERROR;
	}
	buffer->reconstruct_pos += len;
	return 0;
}

int
DCB_full_add_copy(CommandBuffer *buffer, off_u64 src_pos, off_u64 ver_pos, off_u32 len, DCB_SRC_ID src_id)
{
	unsigned long index;
	DCB_full *dcb = (DCB_full *)buffer->DCB;
	if(dcb->cl.com_count == dcb->cl.com_size) {
		if(internal_DCB_resize_cl(&dcb->cl))
			return MEM_ERROR;
	}
	if(buffer->srcs[src_id].type & DCB_DCB_SRC) {
		index = src_pos / buffer->srcs[src_id].src_ptr.dcb->s->quanta;
		assert(index < buffer->srcs[src_id].src_ptr.dcb->s->index_size);
		assert(src_pos + len >= buffer->srcs[src_id].src_ptr.dcb->s->ver_start[index]);

		// ugly.
		if(DCB_rec_copy_from_DCB_src(buffer, &dcb->cl,
			buffer->srcs[src_id].src_ptr.dcb->src_dcb,
			&((DCB_full *)buffer->srcs[src_id].src_ptr.dcb->src_dcb->DCB)->cl,
			buffer->srcs[src_id].src_ptr.dcb->src_map,
			buffer->srcs[src_id].src_ptr.dcb->s->index[index],
			src_pos - buffer->srcs[src_id].src_ptr.dcb->s->ver_start[index], len)) {
			return MEM_ERROR;
		}
			
		// ensure commands were copied in.
		assert(dcb->command_pos < dcb->cl.com_count);
			
		// kind of dumb, but prefer to have DCBufferIncr still being used, should it ever do anything
		// fancy (check wise)
		dcb->command_pos = dcb->cl.com_count - 1;
			
	} else {
		dcb->cl.command[dcb->command_pos].offset = src_pos;
		dcb->cl.command[dcb->command_pos].len = len;
		dcb->cl.src_id[dcb->command_pos] = src_id;
		dcb->cl.com_count++;
	}
	buffer->reconstruct_pos += len;

	DCBufferIncr(buffer);
	return 0;
}

int
DCB_matches_add_copy(CommandBuffer *buffer, off_u64 src_pos, off_u64 ver_pos, off_u32 len, DCB_SRC_ID src_id)
{
	DCB_matches *dcb = (DCB_matches *)buffer->DCB;
	if(dcb->buff_count == dcb->buff_size) {
		internal_DCB_matches_resize(dcb);
	}
	assert(dcb->buff_count == (dcb->cur - dcb->buff));
	dcb->cur->src_pos = src_pos;
	dcb->cur->ver_pos = ver_pos;
	dcb->cur->len = len;
	dcb->buff_count++;
	buffer->reconstruct_pos = ver_pos + len;

	DCBufferIncr(buffer);
	return 0;
}

int
DCB_llm_add_copy(CommandBuffer *buffer, off_u64 src_pos, off_u64 ver_pos, off_u32 len, DCB_SRC_ID src_id)
{
	DCB_llm *dcb = (DCB_llm *)buffer->DCB;
	assert((DCB_LLM_FINALIZED & dcb->flags)==0);
	assert(dcb->buff_count <= dcb->buff_size);
	if(dcb->buff_count == dcb->buff_size) {
		internal_DCB_llm_resize(dcb);
	}
	assert(dcb->buff_count == (dcb->cur - dcb->buff));
	assert(ver_pos + len <= buffer->ver_size);
	dcb->cur->src_pos = src_pos;
	dcb->cur->ver_pos = ver_pos;
	dcb->cur->len = len;
	dcb->buff_count++;
	buffer->reconstruct_pos = ver_pos + len;

	DCBufferIncr(buffer);
	return 0;
}

static int
internal_DCB_llm_free_resize(DCB_llm *buff)
{
	if((buff->free = (void **)realloc(buff->free, buff->free_size * 2 * sizeof(void *)))==NULL) {
		return MEM_ERROR;
	}
	buff->free_size *= 2;
	return 0;
}

static int
internal_DCB_llm_resize(DCB_llm *buff)
{
	unsigned long x;
	assert(buff->buff_count <= buff->buff_size);
	v3printf("resizing ll_matches buffer from %u to %u\n", buff->buff_size, buff->buff_size * 2);
	buff->buff_size *= 2;
	if((buff->buff = (LL_DCLmatch *)realloc(buff->buff, buff->buff_size * sizeof(LL_DCLmatch))) == NULL) {
		return MEM_ERROR;
	}
	buff->cur = buff->buff + buff->buff_count;
	for(x=buff->buff_count; x < buff->buff_size; x++) {
		buff->buff[x].next = NULL;
		buff->buff[x].len = 0;
		buff->buff[x].src_pos = buff->buff[x].ver_pos = 0;
	}
	return 0;
}

void
DCB_full_collapse_adds(DCB_full *dcb)
{
/*	unsigned long count, *plen;
	unsigned int continued_add;
	count = buffer->DCB.full.buffer_count;
	buffer->DCB.full.lb_tail = buffer->DCB.full.lb_start;
	buffer->DCB.full.cb_tail = buffer->DCB.full.cb_head;
	buffer->DCB.full.cb_tail_bit = buffer->DCB.full.cb_head_bit;
	continued_add=0;
	plen = NULL;
	if(buffer->DCBtype==DCBUFFER_FULL_TYPE){
		while(count--) {
			if((*buffer->DCB.full.cb_tail & (1 << 
				buffer->DCB.full.cb_tail_bit))==DC_ADD) {
				if(continued_add) {
					*plen += buffer->DCB.full.lb_tail->len;
					buffer->DCB.full.lb_tail->len = 0;
				} else {
					continued_add = 1;
					plen = &buffer->DCB.full.lb_tail->len;
				}
			} else {
				continued_add=0;
			}
			DCBufferIncr(buffer);
		}
	}
*/
}


void
DCB_full_reset(void *dcb)
{
	((DCB_full *)dcb)->command_pos = 0;
}

void 
DCB_matches_reset(void *dcb)
{
	((DCB_matches *)dcb)->cur = ((DCB_matches *)dcb)->buff;
}

void 
DCB_llm_reset(void *dcb)
{
	DCB_llm *dcb_llm = (DCB_llm *)dcb;
	assert(DCB_LLM_FINALIZED & dcb_llm->flags);
	dcb_llm->main = dcb_llm->main_head;
	dcb_llm->gap_pos = dcb_llm->ver_start;
}


int
DCB_full_commands_remain(void *dcb)
{
	return ((DCB_full*)dcb)->command_pos < ((DCB_full*)dcb)->cl.com_count;
}


void 
DCB_full_free(void *dcb)
{
	CL_free(&((DCB_full *)dcb)->cl);
	((DCB_full *)dcb)->command_pos = 0;
}

void
DCB_matches_free(void *dcb)
{
	free( ((DCB_matches *)dcb)->buff);
	((DCB_matches *)dcb)->buff = NULL;
}

void
DCB_llm_free(void *dcb)
{
	unsigned long x; 
	DCB_llm *dcb_llm = (DCB_llm *)dcb;
	for(x=0; x< dcb_llm->free_count; x++)
		free(dcb_llm->free[x]);
	free(dcb_llm->free);
	dcb_llm->free = NULL;
}


void 
DCBufferFree(CommandBuffer *buffer)
{
	unsigned long x;
	if(buffer->free) buffer->free(buffer->DCB);
	free(buffer->DCB);
	for(x=0; x < buffer->src_count; x++) {
		if(buffer->srcs[x].flags & DCB_FREE_SRC_CFH) {
			v2printf("cclosing src_cfh(%lu)\n", x);
			cclose(buffer->srcs[x].src_ptr.cfh);
			v2printf("freeing  src_cfh(%lu)\n", x);
			free(buffer->srcs[x].src_ptr.cfh);
		}
		if(buffer->srcs[x].type & DCB_DCB_SRC) {
			if(buffer->srcs[x].src_ptr.dcb->s) {
				free_DCBSearch_index(buffer->srcs[x].src_ptr.dcb->s);
			}
			free(buffer->srcs[x].src_ptr.dcb);
			buffer->srcs[x].src_ptr.dcb = NULL;
		}
		if(buffer->srcs[x].ov) {
			free(buffer->srcs[x].ov->command);
			free(buffer->srcs[x].ov->src_id);
			free(buffer->srcs[x].ov);
			buffer->srcs[x].ov = NULL;
		}
	}
	free(buffer->srcs);
	buffer->src_count = buffer->src_array_size = 0;
}

int 
DCB_common_init(CommandBuffer *buffer, unsigned long buffer_size, 
	off_u64 src_size, off_u64 ver_size, unsigned char type)
{
	buffer->flags =0;
	buffer->src_size = src_size;
	buffer->ver_size = ver_size;
	buffer->reconstruct_pos = 0;
	buffer->DCBtype = type;

#ifdef DEBUG_DCBUFFER
	buffer->total_copy_len = 0;
#endif

	buffer->src_count = 0;
	buffer->src_array_size = 4;
	buffer->add_copy = NULL;
	buffer->add_add = NULL;
	buffer->commands_remain = NULL;
	buffer->reset = NULL;
	buffer->free = NULL;
	buffer->finalize = NULL;
	buffer->incr = NULL;
	buffer->decr = NULL;
	buffer->truncate = NULL;
	buffer->DCB = NULL;
	if((buffer->srcs = (DCB_registered_src *)malloc(sizeof(DCB_registered_src) * buffer->src_array_size))==NULL) {
		return MEM_ERROR;
	}
	return 0;
}
int 
DCB_no_buff_init(CommandBuffer *buffer, unsigned long buffer_size, off_u64 src_size, off_u64 ver_size, cfile *out_cfh)
{
	DCB_no_buff *dcb;
	if(DCB_common_init(buffer, buffer_size, src_size, ver_size, DCBUFFER_BUFFERLESS_TYPE))
		return MEM_ERROR;
	else if ((dcb = (DCB_no_buff*)malloc(sizeof(DCB_no_buff))) == NULL) {
		free(buffer->srcs);
		buffer->srcs = NULL;
		return MEM_ERROR;
	}
	dcb->out_cfh = out_cfh;

	buffer->DCB = (void *)dcb;

	buffer->add_add = DCB_no_buff_add_add;
	buffer->add_copy = DCB_no_buff_add_copy;

	return 0;
}

int
DCB_full_init(CommandBuffer *buffer, unsigned long buffer_size, off_u64 src_size, off_u64 ver_size)
{
	DCB_full *dcb;
	if(DCB_common_init(buffer, buffer_size, src_size, ver_size, DCBUFFER_FULL_TYPE))
		return MEM_ERROR;
	else if((dcb = (DCB_full *)malloc(sizeof(DCB_full))) == NULL) {
		free(buffer->srcs);
		buffer->srcs = NULL;
		return MEM_ERROR;
	}		
	if(CL_init(&dcb->cl, 0, 1024, 1)) {
		free(buffer->srcs);
		buffer->srcs = NULL;
		free(dcb);
		return MEM_ERROR;
	}
	dcb->command_pos = 0;
	buffer->DCB = (void *)dcb;

	buffer->add_add = DCB_full_add_add;
	buffer->add_copy = DCB_full_add_copy;
	buffer->incr = DCB_full_incr;
	buffer->decr = DCB_full_decr;
	buffer->reset = DCB_full_reset;
	buffer->get_next = DCB_full_get_next;
	buffer->free = DCB_full_free;
	buffer->commands_remain = DCB_full_commands_remain;
	buffer->truncate = DCB_full_truncate;

	return 0;
}

int
DCB_matches_init(CommandBuffer *buffer, unsigned long buffer_size, off_u64 src_size, off_u64 ver_size)
{
	DCB_matches *dcb;
	if(DCB_common_init(buffer, buffer_size, src_size, ver_size, DCBUFFER_MATCHES_TYPE))
		return MEM_ERROR;
	else if((dcb = (DCB_matches *)malloc(sizeof(DCB_matches))) == NULL) {
		free(buffer->srcs);
		buffer->srcs = NULL;
		return MEM_ERROR;
	}		
	if((dcb->buff = (DCLoc_match *)malloc(buffer_size * sizeof(DCLoc_match)) ) == NULL) {
		free(buffer->srcs);
		buffer->srcs = NULL;
		free(dcb);
		return MEM_ERROR;
	}
	dcb->cur = dcb->buff;
	dcb->buff_size = buffer_size;
	dcb->buff_count = 0;
	buffer->DCB = (void *)dcb;

	buffer->add_copy = DCB_matches_add_copy;
	buffer->incr = DCB_matches_incr;
	buffer->decr = DCB_matches_decr;
	buffer->reset = DCB_matches_reset;
	buffer->get_next = DCB_matches_get_next;
	buffer->free = DCB_matches_free;
	buffer->truncate = DCB_matches_truncate;

	return 0;
}

int
DCB_llm_init(CommandBuffer *buffer, unsigned long buffer_size, off_u64 src_size, off_u64 ver_size)
{
	DCB_llm *dcb;
	if(DCB_common_init(buffer, buffer_size, src_size, ver_size, DCBUFFER_LLMATCHES_TYPE))
		return MEM_ERROR;
	else if((dcb = (DCB_llm *)malloc(sizeof(DCB_llm))) == NULL) {
		free(buffer->srcs);
		buffer->srcs = NULL;
		return MEM_ERROR;
	}		
	dcb->main = dcb->main_head = NULL;
	if((dcb->free = (void **)malloc(10 * sizeof(void *))) == NULL) {
		free(buffer->srcs);
		buffer->srcs = NULL;
		free(dcb);
		return MEM_ERROR;
	}
	dcb->free_size = 10;
	dcb->free_count = 0;
	dcb->buff_count = dcb->main_count = dcb->buff_size = 0;
	dcb->ver_start = 0;
	dcb->flags = DCB_LLM_FINALIZED;
	buffer->DCB = (void *)dcb;

	dcb->buff = dcb->cur = NULL;
	
	buffer->add_copy = DCB_llm_add_copy;
	buffer->incr = DCB_llm_incr;
	buffer->decr = DCB_llm_decr;
	buffer->reset = DCB_llm_reset;
	buffer->get_next = DCB_llm_get_next;
	buffer->free = DCB_llm_free;
	buffer->truncate = DCB_llm_truncate;
	buffer->finalize = DCB_llm_finalize;

	return 0;
}

static int
internal_DCB_matches_resize(DCB_matches *dcb)
{
	v1printf("resizing matches buffer from %u to %u\n", dcb->buff_size, dcb->buff_size * 2);
	dcb->buff_size *= 2;
	if((dcb->buff = (DCLoc_match *)realloc(dcb->buff, dcb->buff_size *		 sizeof(DCLoc_match))) == NULL) {
		return MEM_ERROR;
	}
	dcb->cur = dcb->buff + dcb->buff_count;
	return 0;
}

static int
internal_DCB_resize_cl(command_list *cl)
{
	assert(cl);
	if((cl->src_id = (unsigned char *)realloc(cl->src_id, sizeof(unsigned char) * cl->com_size * 2)) == NULL)
			return MEM_ERROR;
	if((cl->command = (DCLoc *)realloc(cl->command, sizeof(DCLoc) * cl->com_size * 2)) == NULL)
			return MEM_ERROR;
	cl->com_size *= 2;
	return 0;
}

unsigned int
DCB_test_llm_main(CommandBuffer *buff)
{
/*	LL_DCLmatch *ptr;
	unsigned long cur_pos, count=0;
	assert(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype);
	if(buff->DCB.llm.main_head != NULL) {
		ptr = buff->DCB.llm.main_head;
		cur_pos = ptr->ver_pos;
		while(ptr != NULL && ptr->next != NULL) {
			assert(ptr->len < MIN(buff->ver_size, buff->src_size));
			assert(ptr->ver_pos + ptr->len <= buff->ver_size);
			assert(ptr->src_pos + ptr->len <= buff->src_size);
			assert(cur_pos <= ptr->ver_pos + ptr->len);
			cur_pos = ptr->ver_pos + ptr->len;
			ptr = ptr->next;
			count++;
		}
	} else {
		v1printf("main_head is empty, can't test it...\n");
	}
*/
	return 1;
}

int
cmp_llmatch(void *dl1, void *dl2)
{
	LL_DCLmatch *d1 = (LL_DCLmatch *)dl1;
	LL_DCLmatch *d2 = (LL_DCLmatch *)dl2;
	if(d1->ver_pos < d2->ver_pos)
		return -1;
	else if(d1->ver_pos > d2->ver_pos)
		return 1;
	if(d1->len == d2->len) 
		return 0;
	else if(d1->len < d2->len)
		return -1;
	return 1;
}

int
DCB_llm_finalize(void *d_ptr)
{
	DCB_llm *dcb = (DCB_llm *)d_ptr;
	unsigned long x;
	if(dcb->buff_count > 0 ) {
		dcb->cur--;
		v2printf("inserting a segment %llu:%llu, commands(%u)\n", 
			(act_off_u64)dcb->buff->ver_pos, (act_off_u64)(LLM_VEND(dcb->cur)), 
			dcb->buff_count);

		assert(dcb->main_head==NULL ? dcb->main == NULL : 1);
		if((dcb->buff = (LL_DCLmatch *)realloc(dcb->buff, dcb->buff_count * sizeof(LL_DCLmatch)))==NULL) {
			return MEM_ERROR;
		}
		// link the buggers
		for(x=0; x < dcb->buff_count -1 ; x++) {
			dcb->buff[x].next = dcb->buff + x +1;
		}
		dcb->cur = dcb->buff + dcb->buff_count -1;
		if(dcb->main_head == NULL) {
			//no commands exist yet
			v2printf("main is empty\n");
			dcb->main_head = dcb->buff;
			dcb->main = dcb->cur;
			dcb->main->next = NULL;
		} else if(dcb->main_head->ver_pos >= dcb->cur->ver_pos) {
			// prepending it
			v2printf("prepending commands\n");
			dcb->cur->next = dcb->main;
			dcb->main_head = dcb->buff;
		} else {
			v2printf("gen. insert\n");
			dcb->cur->next = dcb->main->next;
			dcb->main->next = dcb->buff;
			dcb->main = dcb->cur;
		}
		dcb->main_count += dcb->buff_count;
		if(dcb->free_count == dcb->free_size) {
			internal_DCB_llm_free_resize(dcb);
		}
		dcb->free[dcb->free_count++] = dcb->buff;
	} else if(!(dcb->flags & DCB_LLM_FINALIZED)){
		free(dcb->buff);
	}
	dcb->buff = dcb->cur = NULL;
	dcb->buff_count = dcb->buff_size = 0;
	dcb->flags |= DCB_LLM_FINALIZED;
	return 0;
}

int
DCB_llm_init_buff(CommandBuffer *buff, unsigned int buff_size)
{
	DCB_llm *dcb = (DCB_llm *)buff->DCB;
	v3printf("llm_init_buff called\n");
	assert(DCBUFFER_LLMATCHES_TYPE == buff->DCBtype);
	assert(dcb->flags & DCB_LLM_FINALIZED);
	if((dcb->buff = (LL_DCLmatch *)malloc(buff_size * sizeof(LL_DCLmatch)) ) == NULL) {
		return MEM_ERROR;
	}
	dcb->cur = dcb->buff;
	dcb->buff_size = buff_size;
	dcb->buff_count = 0;
	dcb->flags &= ~DCB_LLM_FINALIZED;
	return 0;
}

DCBSearch *
create_DCBSearch_index(CommandBuffer *dcb)
{
	unsigned long pos, ver_pos, dpos, tmp_len;
	DCBSearch *s;
	DCB_full *dcbf = (DCB_full *)dcb->DCB;

	if(dcb->DCBtype != DCBUFFER_FULL_TYPE)
			return NULL;

	if(! dcb->ver_size) {
		assert(dcb->ver_size);
			return NULL;
	}

	s = malloc(sizeof(DCBSearch));

	if(s == NULL) 
			return NULL;

	// basically, take a rough guess at the avg command len, use it to determine the divisor, then adjust index_size
	// to not allocate uneeded space (due to rounding of original index_size and quanta)
	if(dcbf->cl.com_count < 2)
			s->index_size = 1;
	else
		s->index_size = ceil(dcbf->cl.com_count / 2);

	v1printf("index_size = %lu\n", s->index_size);
	s->quanta = ceil(dcb->ver_size / s->index_size);
	s->index_size = ceil(dcb->ver_size / s->quanta) + 1;
	s->index = (unsigned long *)malloc(sizeof(unsigned long) * s->index_size);

	if(s->index == NULL) {
		free(s);
		return NULL;
	}

	s->ver_start = (off_u64 *)malloc(sizeof(off_u64) * s->index_size);
	if(s->ver_start == NULL) {
			free(s->index);
			free(s);
			return NULL;
	}
	pos=0;
	dpos = 0;
	ver_pos = 0;
	while(dpos < dcbf->cl.com_count) {
		if(dcb->srcs[dcbf->cl.src_id[dpos]].ov) {
			tmp_len = dcb->srcs[dcbf->cl.src_id[dpos]].ov->command[dcbf->cl.command[dpos].offset].len;
		} else {
			tmp_len = dcbf->cl.command[dpos].len;
		}
		ver_pos += tmp_len;
			while(ver_pos > pos * s->quanta && pos < s->index_size) {
				s->index[pos] = dpos;
				s->ver_start[pos] = ver_pos - tmp_len;
				pos++;
			}
			dpos++;
	}
	return s;	
}

void
free_DCBSearch_index(DCBSearch *s)
{
	if(s) {
			free(s->index);
			free(s->ver_start);
			free(s);
	}
}

