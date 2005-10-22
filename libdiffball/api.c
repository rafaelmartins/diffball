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
#include <diffball/apply-patch.h>
#include <diffball/errors.h>

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
	DCBufferFree(&buffer);
	return encode_result;
}

int
simple_reconstruct(cfile *src_cfh, cfile **patch_cfh, unsigned char patch_count, cfile *out_cfh, unsigned int force_patch_id, 
	unsigned int max_buff_size)
{
	CommandBuffer dcbuff[2];
	unsigned long x;
	signed int src_id;
	signed int err;
	unsigned char reorder_commands = 0;
	unsigned char bufferless = 1;
	unsigned long int patch_id[256];
	signed long int recon_val=0;
	
	if(max_buff_size == 0)
		max_buff_size = 0x40000;
	
	/* currently, unwilling to do bufferless for more then one patch.  overlay patches are the main 
	   concern; it shouldn't be hard completing the support, just no motivation currently :) */
	
	for(x=0; x < patch_count; x++) {
		if(patch_cfh[x]->compressor_type != NO_COMPRESSOR) {
			reorder_commands = 1;
		}

		if(force_patch_id == 0) {
			patch_id[x] = identify_format(patch_cfh[x]);
			if(patch_id[x]==0) {
				v1printf( "Couldn't identify the patch format for patch %lu, aborting\n", patch_id[x]);
				return UNKNOWN_FORMAT;
			} else if((patch_id[x] & 0xffff)==1) {
				v0printf( "Unsupported format version\n");
				return UNKNOWN_FORMAT;
			}
			patch_id[x] >>=16;
		} else {
			patch_id[x] = force_patch_id;
		}
		v1printf("patch_type=%lu\n", patch_id[x]);
	}

	if(src_cfh->compressor_type != NO_COMPRESSOR) {
			reorder_commands = 1;
	}

	if(patch_count == 1 && reorder_commands == 0) {
		bufferless = 1;
		v1printf("enabling bufferless, patch_count(%lu) == 1\n", patch_count);
	} else {
		bufferless = 0;
		v1printf("disabling bufferless, patch_count(%lu) == 1 || forced_reorder(%u)\n", patch_count, reorder_commands);
	}


	#define ret_error(err, msg)				\
	if(err)	{ 								\
		DCBufferFree(&dcbuff[(x - 1) % 2]);	\
		check_return_ret(err, 1, msg);		\
	}

	for(x=0; x < patch_count; x++) {
		if(x == patch_count - 1 && reorder_commands == 0 && bufferless) {
			v1printf("not reordering, and bufferless is %u, going bufferless\n", bufferless);
			if(x==0) {
				err=DCB_no_buff_init(&dcbuff[0], 0, cfile_len(src_cfh), 0, out_cfh);
				ret_error(err, "DCB_no_buff_init failure");
				src_id = internal_DCB_register_cfh_src(&dcbuff[0], src_cfh, NULL, NULL, DC_COPY, 0);
				ret_error(src_id, "DCB_no_buff_init failure");
			} else {
				err=DCB_no_buff_init(&dcbuff[x % 2], 0, dcbuff[(x - 1) % 2].ver_size, 0, out_cfh);
				ret_error(err, "DCB_no_buff_init failure");
				src_id = DCB_register_dcb_src(dcbuff + ( x % 2), dcbuff + ((x -1) % 2));
				ret_error(src_id, "internal_DCB_register_src");
			}

		} else {
			if(x==0) {
				err=DCB_full_init(&dcbuff[0], 4096, 0, 0);
				ret_error(err, "DCB_full_init failure");
				src_id = internal_DCB_register_cfh_src(&dcbuff[0], src_cfh, NULL, NULL, DC_COPY, 0);
				ret_error(src_id, "src_id registration failure");
			} else {
				err=DCB_full_init(&dcbuff[x % 2], 4096, dcbuff[(x - 1) % 2].ver_size , 0);
				ret_error(err, "DCBufferInit");
				src_id = DCB_register_dcb_src(dcbuff + ( x % 2), dcbuff + ((x -1) % 2));
				ret_error(src_id, "DCB_register_dcb_src");
			}
		}

		if(SWITCHING_FORMAT == patch_id[x]) {
			recon_val = switchingReconstructDCBuff(src_id, patch_cfh[x], &dcbuff[x % 2]);
		} else if(GDIFF4_FORMAT == patch_id[x]) {
			recon_val = gdiff4ReconstructDCBuff(src_id, patch_cfh[x], &dcbuff[x % 2]);
		} else if(GDIFF5_FORMAT == patch_id[x]) {
			recon_val = gdiff5ReconstructDCBuff(src_id, patch_cfh[x], &dcbuff[x % 2]);
		} else if(BDIFF_FORMAT == patch_id[x]) {
			recon_val = bdiffReconstructDCBuff(src_id, patch_cfh[x], &dcbuff[x % 2]);
		} else if(XDELTA1_FORMAT == patch_id[x]) {
			recon_val = xdelta1ReconstructDCBuff(src_id, patch_cfh[x], &dcbuff[x % 2], 1);

			/* this could be adjusted a bit, since with xdelta it's optional to compress the patch
			   in such a case, the decision for reordering shouldn't be strictly controlled here */
			if(patch_count > 1)
				reorder_commands = 1;

		} else if(BDELTA_FORMAT == patch_id[x]) {
			recon_val = bdeltaReconstructDCBuff(src_id, patch_cfh[x], &dcbuff[x % 2]);
		} else if(BSDIFF_FORMAT == patch_id[x]) {
			recon_val = bsdiffReconstructDCBuff(src_id, patch_cfh[x], &dcbuff[x % 2]);

			/* compressed format- if more then one patch, reorder
			   besides that, currently multiple bsdiff patches are supported only via 
			   read_seq_write_rand.  needs fixing later on */
			if(patch_count > 1)
				reorder_commands = 1;

		} else if(FDTU_FORMAT == patch_id[x]) {
			recon_val = fdtuReconstructDCBuff(src_id, patch_cfh[x], &dcbuff[x % 2]);

			/* wrapped xdelta format, same reasoning applies */
			if(patch_count > 1)
				reorder_commands = 1;

//		} else if(UDIFF_FORMAT == patch_id[x]) {
//			recon_val = udiffReconstructDCBuff(src_id, &patch_cfh[x], src_cfh, NULL, &dcbuff[x % 2]);
		}


		v1printf("reconstruction return=%ld", recon_val);
		if(DCBUFFER_FULL_TYPE == dcbuff[x % 2].DCBtype) {
			v1printf(", commands=%ld\n", ((DCB_full *)dcbuff[x % 2].DCB)->cl.com_count);
			v1printf("result was %lu commands\n", ((DCB_full *)dcbuff[x % 2].DCB)->cl.com_count);
		} else {
			v1printf("\n");
		}
		if(x) {
			DCBufferFree(&dcbuff[(x - 1) % 2]);
		}
		if(recon_val) {
			v1printf("error detected while processing patch- quitting\n");
			DCBufferFree(&dcbuff[x % 2]);
			check_return_ret(recon_val, 1, "reconstruct result");
		}
		v1printf("versions size is %llu\n", (act_off_u64)dcbuff[x % 2].ver_size);
	}
	v1printf("applied %lu patches\n", patch_count);

	if(! bufferless) {
		v1printf("reordering commands? %u\n", reorder_commands);
		v1printf("reconstructing target file based off of dcbuff commands...\n");
		err = reconstructFile(&dcbuff[(patch_count - 1) % 2], out_cfh, reorder_commands, max_buff_size);
		check_return(err, "reconstructFile", "error detected while reconstructing file, quitting");	
		v1printf("reconstruction completed successfully\n");
	} else {
		v1printf("reconstruction completed successfully\n");
	}

	DCBufferFree(&dcbuff[(patch_count - 1) % 2]);
	return 0;
}

