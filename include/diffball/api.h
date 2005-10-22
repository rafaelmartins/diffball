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

#ifndef HEADER_API_
#define HEADER_API_ 1

int
simple_difference(cfile *ref, cfile *ver, cfile *out, unsigned int patch_id, unsigned long seed_len,
    unsigned long sample_rate, unsigned long hash_size);

int
simple_reconstruct(cfile *src_cfh, cfile *patch_cfh[], unsigned char patch_count, cfile *out_cfh, unsigned int force_patch_id,
    unsigned int max_buff_size);
        
#endif
