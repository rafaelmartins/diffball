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
#define DC_ADD 0
#define DC_COPY 1
#define ENCODING_OFFSET_START 0
#define ENCODING_OFFSET_VERS_POS 1
#define ENCODING_OFFSET_DC_POS 2
#define USE_GDIFF_ENCODING 0
#define LOOKBACK_SIZE 100000

typedef struct {
    unsigned long offset;
    unsigned long len;
} DCLoc;

typedef struct {
    unsigned long flush_count;
    unsigned long buffer_count;
    unsigned long buffer_size;
    unsigned long src_size;
    unsigned long ver_size;
    unsigned long reconstruct_pos;
    unsigned char *cb_start, *cb_end, *cb_head, *cb_tail;
    unsigned char cb_tail_bit, cb_head_bit;
    DCLoc *lb_start, *lb_end, *lb_head, *lb_tail;
} CommandBuffer;

typedef struct {
    unsigned long copy_count;
    unsigned long add_count;
    unsigned long copy_pos_offset_bytes[5];
    unsigned long copy_rel_offset_bytes[5];
    unsigned long copy_len_bytes[5];
} DCStats;

unsigned long inline current_command_type(CommandBuffer *buff);
void updateDCCopyStats(DCStats *stats, signed long pos_offset, signed long dc_offset, unsigned long len);
void DCBufferIncr(CommandBuffer *buffer);
void DCBufferDecr(CommandBuffer *buffer);
void DCBufferCollapseAdds(CommandBuffer *buffer);
void DCBufferAddCmd(CommandBuffer *buffer, int type, unsigned long offset, unsigned long len);
void DCBufferTruncate(CommandBuffer *buffer, unsigned long len);
void DCBufferInit(CommandBuffer *buffer, unsigned long buffer_size,
    unsigned long src_size, unsigned long ver_size);
unsigned long DCBufferReset(CommandBuffer *buffer);
#endif
