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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "dcbuffer.h"
#include "cfile.h"
#include "bit-functions.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

unsigned long inline get_current_command_type(struct CommandBuffer *buff) {
	return ((*buff->cb_tail >> buff->cb_tail_bit) & 0x01);
}

void updateDCCopyStats(struct DCStats *stats, signed long pos_offset, signed long dc_offset, unsigned long len)
{
    stats->copy_count++;
    stats->copy_pos_offset_bytes[MAX( MIN(signedBytesNeeded(pos_offset) - 1, 0), 5)]++;
    stats->copy_rel_offset_bytes[MAX(MIN(signedBytesNeeded(dc_offset) -1, 0), 5)]++;
    stats->copy_len_bytes[MAX(MIN(unsignedBytesNeeded(len)-1, 0),5)]++;
}

void updateDCAddStats(struct DCStats *stats, unsigned long len)
{
    stats->add_count++;
    
}

void undoDCCopyStats(struct DCStats *stats, signed long pos_offset, unsigned long len)
{
    stats->copy_count++;
    stats->copy_pos_offset_bytes[MAX( MIN(signedBytesNeeded(pos_offset) - 1, 0), 5)]--;
//    stats->copy_rel_offset_bytes[MAX(MIN(signedBytesNeeded(dc_offset) -1, 0), 5)]--;
    stats->copy_len_bytes[MAX(MIN(unsignedBytesNeeded(len)-1, 0),5)]--;
}

void undoDCAddStats(struct DCStats *stats, unsigned long len)
{
    stats->add_count--;
    
}

void DCBufferTruncate(struct CommandBuffer *buffer, unsigned long len)
{
    //get the tail to an actual node.
    DCBufferDecr(buffer);
    //printf("truncation: \n");
    while(len) {
		/* should that be less then or equal? */
		if (buffer->lb_tail->len <= len) {
		    len -= buffer->lb_tail->len;
//		    printf("    whole removal of type(%u), offset(%lu), len(%lu)\n",
//			(*buffer->cb_tail & (1 << buffer->cb_tail_bit)) >> buffer->cb_tail_bit, buffer->lb_tail->offset,
//			buffer->lb_tail->len);
		    DCBufferDecr(buffer);
		    buffer->count--;
		} else {
//		    printf("    partial adjust of type(%u), offset(%lu), len(%lu) is now len(%lu)\n",
//			(*buffer->cb_tail & (1 << buffer->cb_tail_bit))>buffer->cb_tail_bit, buffer->lb_tail->offset,
//			buffer->lb_tail->len, buffer->lb_tail->len - len);
		    buffer->lb_tail->len -= len;
		    len=0;
		}
    }
    DCBufferIncr(buffer);
}


void DCBufferIncr(struct CommandBuffer *buffer)
{
    buffer->lb_tail = (buffer->lb_end==buffer->lb_tail) ? buffer->lb_start : buffer->lb_tail + 1;
    if (buffer->cb_tail_bit >= 7) {
	buffer->cb_tail_bit = 0;
	buffer->cb_tail = (buffer->cb_tail == buffer->cb_end) ? buffer->cb_start : buffer->cb_tail + 1;
    } else {
	buffer->cb_tail_bit++;
    }
}

void DCBufferDecr(struct CommandBuffer *buffer)
{
    buffer->lb_tail--;
    if (buffer->cb_tail_bit != 0) {
	buffer->cb_tail_bit--;
    } else {
	buffer->cb_tail = (buffer->cb_tail == buffer->cb_start) ? buffer->cb_end : buffer->cb_tail - 1;
	buffer->cb_tail_bit=7;
    }
}

void DCBufferAddCmd(struct CommandBuffer *buffer, int type, unsigned long offset, unsigned long len)
{
    if(buffer->count == buffer->max_commands) {
		printf("shite, buffer full.\n");
		exit(EXIT_FAILURE);
    }
    buffer->lb_tail->offset = offset;
    buffer->lb_tail->len = len;
    if (type==DC_ADD)
		*buffer->cb_tail &= ~(1 << buffer->cb_tail_bit);
    else
		*buffer->cb_tail |= (1 << buffer->cb_tail_bit);
    buffer->count++;
    DCBufferIncr(buffer);
}

void DCBufferCollapseAdds(struct CommandBuffer *buffer)
{
	unsigned long count, *plen;
	unsigned int continued_add;
	count = buffer->count;
	buffer->lb_tail = buffer->lb_start;
	buffer->cb_tail = buffer->cb_head;
	buffer->cb_tail_bit = buffer->cb_head_bit;
	continued_add=0;
	plen = NULL;
	while(count--) {
	    if((*buffer->cb_tail & (1 << buffer->cb_tail_bit))==DC_ADD) {
		if(continued_add) {
		    *plen += buffer->lb_tail->len;
		    buffer->lb_tail->len = 0;
		} else {
		    continued_add = 1;
		    plen = &buffer->lb_tail->len;
		}
	    } else {
		continued_add=0;
	    }
	    DCBufferIncr(buffer);
	}
}

void DCBufferInit(struct CommandBuffer *buffer, unsigned long max_commands)
{
    buffer->count=0;
    buffer->max_commands = max_commands + (max_commands % 8 ? 1 : 0);
//    printf("asked for size(%lu), using size(%lu)\n", max_commands, buffer->max_commands);
    if((buffer->cb_start = (char *)malloc(buffer->max_commands/8))==NULL){
	perror("shite, malloc failed\n");
	exit(EXIT_FAILURE);
    }
    buffer->cb_head = buffer->cb_tail = buffer->cb_start;
    buffer->cb_end = buffer->cb_start + (buffer->max_commands/8) -1;
    buffer->cb_head_bit = buffer->cb_tail_bit = 0;
    if((buffer->lb_start = (struct DCLoc *)malloc(sizeof(struct DCLoc) * buffer->max_commands))==NULL){
	perror("shite, malloc failed\n");
	exit(EXIT_FAILURE);
    }
    buffer->lb_head = buffer->lb_tail = buffer->lb_start;
    buffer->lb_end = buffer->lb_start + buffer->max_commands -1;
}

