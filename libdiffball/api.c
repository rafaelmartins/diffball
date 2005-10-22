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
#include <cfile.h>
#include <diffball/diff-algs.h>
#include <diffball/formats.h>
#include <diffball/defs.h>


int
simple_difference(cfile *ref_cfh, cfile *ver_cfh, cfile *out_cfh, unsigned int patch_id, unsigned long seed_len, 
	unsigned long sample_rate, unsigned long hash_size)
{
	CommandBuffer buffer;
	int encode_result;
	EDCB_SRC_ID ref_id, ver_id;
	if(hash_size==0) {
		/* implement a better assessment based on mem and such */
		hash_size = MIN(DEFAULT_MAX_HASH_COUNT, cfile_len(ref_cfh));
	}
	if(sample_rate==0) {
		/* implement a better assessment based on mem and such */
		sample_rate = COMPUTE_SAMPLE_RATE(hash_size, cfile_len(ref_cfh));
	}
	if(seed_len==0) {
		seed_len = DEFAULT_SEED_LEN;
	}

	if(patch_id==0) {
		patch_id = DEFAULT_PATCH_ID;
	}


	DCB_llm_init(&buffer, 4, cfile_len(ref_cfh), cfile_len(ver_cfh));
	ref_id = DCB_REGISTER_ADD_SRC(&buffer, ver_cfh, NULL, 0);
	ver_id = DCB_REGISTER_COPY_SRC(&buffer, ref_cfh, NULL, 0);
	MultiPassAlg(&buffer, ref_cfh, ref_id, ver_cfh, ver_id, hash_size);
	DCB_finalize(&buffer);
	DCB_test_total_copy_len(&buffer);
	if(GDIFF4_FORMAT == patch_id) {
		encode_result = gdiff4EncodeDCBuffer(&buffer, out_cfh);
	} else if(GDIFF5_FORMAT == patch_id) {
		encode_result = gdiff5EncodeDCBuffer(&buffer, out_cfh);
	} else if(BDIFF_FORMAT == patch_id) {
		encode_result = bdiffEncodeDCBuffer(&buffer, out_cfh);
	} else if(SWITCHING_FORMAT == patch_id) {
		encode_result = switchingEncodeDCBuffer(&buffer, out_cfh);
	} else if (BDELTA_FORMAT == patch_id) {
		encode_result = bdeltaEncodeDCBuffer(&buffer, out_cfh);
	}
	return encode_result;
}
