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
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "string-misc.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "tar.h"
#include "data-structs.h"
#include "cfile.h"
#include "dcbuffer.h"
#include "diff-algs.h"
#include "formats.h"
#include "defs.h"
#include "options.h"

unsigned int global_verbosity = 0;
unsigned int global_use_md5 = 0;

unsigned int src_common_len=0, trg_common_len=0;

int cmp_ver_tar_ent_to_src_tar_ent(const void *te1, const void *te2);
int cmp_tar_ents(const void *te1, const void *te2);

unsigned long sample_rate=0;
unsigned long seed_len = 0;
unsigned long hash_size = 0;
unsigned int patch_compressor = 0;
unsigned int patch_to_stdout = 0;
char  *patch_format;

struct poptOption options[] = {
    STD_OPTIONS(patch_to_stdout),
    DIFF_OPTIONS(seed_len, sample_rate, hash_size),
    FORMAT_OPTIONS("patch-format", 'f', patch_format),
    POPT_TABLEEND
};


int 
main(int argc, char **argv)
{
    int src_fh, trg_fh, out_fh;
    tar_entry *source = NULL;
    tar_entry **src_ptrs = NULL;
    tar_entry *target = NULL;
    tar_entry *tar_ptr = NULL;
    void *vptr;
    unsigned char ref_id, ver_id;
    unsigned long source_count, target_count;
    signed long encode_result=0;
    unsigned long x, patch_format_id;
    char src_common[512], trg_common[512], *p;  /* common dir's... */
    unsigned long match_count;
	
    cfile ref_full, ref_window, ver_window, ver_full, out_cfh;
    struct stat ref_stat, ver_stat;
    RefHash rhash_win;
    CommandBuffer dcbuff;
    poptContext p_opt;

    signed long optr;
    char  *src_file;
    char  *trg_file;
    char  *patch_name;

    p_opt = poptGetContext("diffball", argc, (const char **)argv, options, 0);
    while((optr=poptGetNextOpt(p_opt)) != -1) {
	if(optr < -1) {
	    usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS), 
		poptStrerror(optr));
	}
	switch(optr) {
	case OVERSION:
	    print_version("diffball");
	case OVERBOSE:
	    global_verbosity++;
	    break;
	case OBZIP2:
	    if(patch_compressor) {
		// bitch at em.
	    } else
		patch_compressor = BZIP2_COMPRESSOR;
	    break;
	case OGZIP:
	    if(patch_compressor) {
		// bitch at em.
	    } else 
		patch_compressor = GZIP_COMPRESSOR;
	    break;
	}
    }
    if( ((src_file=(char *)poptGetArg(p_opt))==NULL) || 
	(stat(src_file, &ref_stat))) 
	usage(p_opt, 1, "Must specify an existing source file.", NULL);
    if( ((trg_file=(char *)poptGetArg(p_opt))==NULL) || 
	(stat(trg_file, &ver_stat)) )
	usage(p_opt, 1, "Must specify an existing target file.", NULL);
    if(patch_format==NULL) {
	patch_format_id = DEFAULT_PATCH_ID;
    } else {
	patch_format_id = check_for_format(patch_format, strlen(patch_format));
	if(patch_format_id==0) {
	    v0printf( "Unknown format '%s'\n", patch_format);
	    exit(1);
	}
    }
    if(patch_to_stdout != 0) {
	out_fh = 1;
    } else {
	if((patch_name = (char *)poptGetArg(p_opt))==NULL)
	    usage(p_opt, 1, "Must specify a name for the patch file.", NULL);
	if((out_fh = open(patch_name, O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
	    v0printf( "error creating patch file (open failed)\n");
	    exit(1);
	}
    }
    if(NULL!=poptGetArgs(p_opt)) {
	usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS),
	"unknown option");
    }
    poptFreeContext(p_opt);
    if(hash_size==0) {
	hash_size = MIN(DEFAULT_MAX_HASH_COUNT, ref_stat.st_size);
    }
    if((src_fh = open(src_file, O_RDONLY,0)) == -1) {
	v0printf( "error opening source file '%s'\n", src_file);
	exit(1);
    }
    if((trg_fh = open(trg_file, O_RDONLY,0)) == -1) {
	v0printf( "error opening target file '%s'\n", trg_file);
	exit(1);
    }

    if(copen(&ref_full, src_fh, 0, ref_stat.st_size, NO_COMPRESSOR, CFILE_RONLY |
	CFILE_OPEN_FH ) ||
        copen(&ver_full, trg_fh, 0, ver_stat.st_size, NO_COMPRESSOR, CFILE_RONLY)) {
	v0printf("error opening file; exiting\n");
	exit(1);
    }

    if(sample_rate==0) {
	sample_rate = COMPUTE_SAMPLE_RATE(hash_size, cfile_len(&ref_full));
    }
    if(seed_len==0) {
	seed_len = DEFAULT_SEED_LEN;
    }

    v1printf("using patch format %lu\n", patch_format_id);
    v1printf("using seed_len(%lu), sample_rate(%lu), hash_size(%lu)\n", 
	seed_len, sample_rate, hash_size);
    v1printf("verbosity level(%u)\n", global_verbosity);

    v1printf("reading tar entries from src\n");
    if(read_fh_to_tar_entry(&ref_full, &source, &source_count))
	exit(EXIT_FAILURE);

    v2printf("reading tar entries from trg\n");
    if(read_fh_to_tar_entry(&ver_full, &target, &target_count))
	exit(EXIT_FAILURE);

    v2printf("source tarball's entry count=%lu\n", source_count);
    v2printf("target tarball's entry count=%lu\n", target_count);

    v3printf("qsorting\n");
    src_ptrs = (tar_entry **)malloc(sizeof(tar_entry *) * source_count);
    if(src_ptrs == NULL) {
    	v0printf("unable to allocate needed memory, bailing\n");
    	exit(EXIT_FAILURE);
    }
    for(x=0; x< source_count; x++)
    	src_ptrs[x] = source + x;
    qsort(src_ptrs, source_count, sizeof(tar_entry *), cmp_tar_ents);
    v3printf("qsort done\n");
    
/* alg to basically figure out the common dir prefix... eg, if everything 
   is in dir debianutils-1.16.3; note, we want the slash, hence +1 */

    p = rindex(src_ptrs[0]->fullname, '/');
    if(p!=NULL) {
	src_common_len = ((char *)p - (char *)src_ptrs[0]->fullname) + 1;
	strncpy((char *)src_common, (char *)src_ptrs[0]->fullname, 
	    src_common_len);
    } else {
	src_common_len = 0;
    }
    src_common[src_common_len] = '\0';  /*null delimit it */

    for (x=0; x < source_count && src_common_len != 0; x++) {
        if (strncmp((const char *)src_common, (const char *)src_ptrs[x]->fullname, src_common_len) !=0) {
            char *p;

            /* null the / at src_common_len-1, and attempt rindex again. */

            src_common[src_common_len -1]= '\0';
            if((p = rindex(src_common, '/'))==NULL){
		/*no common dir prefix. damn. */
                src_common_len=0;
                src_common[0]='\0'; 
            } else {
		/*include the / in the path again... */
                src_common_len= p - src_common  +1; 
                src_common[src_common_len]='\0';
            }
        }
    }
    v1printf("final src_common='%.*s'\n", src_common_len, src_common);
    p = rindex(target[0].fullname, '/');
    if(p!=NULL) {
	trg_common_len = ((char *)p - (char *)target[0].fullname) + 1;
	strncpy((char *)trg_common, (char *)target[0].fullname, trg_common_len);
    } else {
    	trg_common_len = 0;
    }
    trg_common[trg_common_len] = '\0';  /* null delimit it */

    for (x=0; x < target_count && trg_common_len != 0; x++) {
        if (strncmp((const char *)trg_common, (const char *)target[x].fullname, 
	    trg_common_len) !=0) {

            /* null the / at trg_common_len-1, and attempt rindex again. */

            trg_common[trg_common_len -1]='\0';
            if((p = rindex(trg_common, '/'))==NULL){
                trg_common_len=0;
                trg_common[0]='\0'; /*no common dir prefix. damn. */
            } else {
                trg_common_len= p - trg_common + 1; /*include the / again... */
                trg_common[trg_common_len]='\0';
            }
        }
    }
    v1printf("final trg_common='%.*s'\n", trg_common_len, trg_common);

    if(DCBufferInit(&dcbuff, 4096, (unsigned long)ref_stat.st_size, 
	(unsigned long)ver_stat.st_size, DCBUFFER_LLMATCHES_TYPE) ||
	DCB_llm_init_buff(&dcbuff, 4096)) {
	v0printf("error allocing needed memory, exiting\n");
	exit(1);
    }
    v1printf("looking for matching filenames in the archives...\n");

    ver_id = DCB_REGISTER_ADD_SRC(&dcbuff, &ver_full, NULL, 0);
    ref_id = DCB_REGISTER_COPY_SRC(&dcbuff, &ref_full, NULL, 0);
    for(x=0; x< target_count; x++) {
//	if(target[x]->file_loc < 1155051 || target[x]->file_loc > 
//	    1155053) {
//	    v0printf("bypassing %lu...\n", target[x]->file_loc);
//	    DCB_add_add(&dcbuff, target[x]->file_loc * 512, 
//	    512 + (target[x]->size==0 ? 0 :
//	                    target[x]->size + 512 - (target[x]->size % 512==0 ?
//	                                    512 : target[x]->size % 512)), ver_id);
//	    continue;
//	} else {
//	    v0printf("processing\n");
//	}

	v1printf("processing %lu of %lu\n", x + 1, target_count);
	tar_ptr = &target[x];
        vptr = bsearch(&tar_ptr, src_ptrs, 
	    source_count, sizeof(tar_entry **), cmp_ver_tar_ent_to_src_tar_ent);
        if(vptr == NULL) {
	    v1printf("didn't find a match for %.255s, skipping\n", 
		target[x].fullname);
        } else {
            tar_ptr = (tar_entry *)*((tar_entry **)vptr);
//	    tar_ptr = (tar_entry *)vptr;
            v1printf("found match between %.255s and %.255s\n", target[x].fullname,
		tar_ptr->fullname);
	    v2printf("differencing src(%lu:%lu) against trg(%lu:%lu)\n",
		tar_ptr->start, tar_ptr->end, target[x].start, target[x].end);
/*        	(512 * tar_ptr->file_loc), (512 * tar_ptr->file_loc) + 512 + 
		(tar_ptr->size==0 ? 0 :	tar_ptr->size + 512 - 
		(tar_ptr->size % 512==0 ? 512: 	tar_ptr->size % 512)),
		(512 * target[x]->file_loc), (512 * target[x]->file_loc) + 
		512 +(target[x]->size==0 ? 0 : target[x]->size + 512 - 
		(target[x]->size % 512==0 ? 512 : target[x]->size % 512) ));
*/
	    copen_child_cfh(&ver_window, &ver_full, target[x].start, target[x].end,
            	NO_COMPRESSOR, CFILE_RONLY | CFILE_BUFFER_ALL);
/*	    (512 * target[x]->file_loc),
            	(512 * target[x]->file_loc) + 512 + (target[x]->size==0 ? 0 :
            	target[x]->size + 512 - (target[x]->size % 512==0 ? 
	    	512 : target[x]->size % 512)),
*/
            copen_child_cfh(&ref_window, &ref_full, tar_ptr->start, tar_ptr->end,
        	NO_COMPRESSOR, CFILE_RONLY | CFILE_BUFFER_ALL);
            
/*            (512 * tar_ptr->file_loc), 
        	(512 * tar_ptr->file_loc) + 512 + (tar_ptr->size==0 ? 0 :
        	tar_ptr->size + 512 - (tar_ptr->size % 512==0 ? 
        	512 : tar_ptr->size % 512)),
*/
            match_count++;
            init_RefHash(&rhash_win, &ref_window, 24, 1, 
		cfile_len(&ref_window), RH_BUCKET_HASH);
	    RHash_insert_block(&rhash_win, &ref_window, 0, 
		cfile_len(&ref_window));
	    RHash_cleanse(&rhash_win);
	    print_RefHash_stats(&rhash_win);
            OneHalfPassCorrecting2(&dcbuff, &rhash_win, ref_id, &ver_window, ver_id);
//	    MultiPassAlg(&dcbuff, &ref_window, &ver_window, hash_size);
            free_RefHash(&rhash_win);

	    cclose(&ver_window);
	    cclose(&ref_window);
        }
    }

    /* cleanup */
    for(x=0; x< source_count; x++)
        free(source[x].fullname);
    free(source);
    free(src_ptrs);
    
    for(x=0; x< target_count; x++)
	free(target[x].fullname);
    free(target);

    v1printf("beginning search for gaps, and unprocessed files\n");
    MultiPassAlg(&dcbuff, &ref_full, ref_id, &ver_full, ver_id, hash_size);
    DCB_insert(&dcbuff);
    cclose(&ref_full);

    copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY | CFILE_OPEN_FH);
    v1printf("outputing patch...\n");
    v1printf("there were %lu commands\n", dcbuff.DCB.full.buffer_count);
    if(GDIFF4_FORMAT == patch_format_id) { 
        encode_result = gdiff4EncodeDCBuffer(&dcbuff, &out_cfh);
    } else if(GDIFF5_FORMAT == patch_format_id) {
        encode_result = gdiff5EncodeDCBuffer(&dcbuff, &out_cfh);
    } else if(BDIFF_FORMAT == patch_format_id) {
        encode_result = bdiffEncodeDCBuffer(&dcbuff, &out_cfh);
    } else if(SWITCHING_FORMAT == patch_format_id) {
	DCBufferCollapseAdds(&dcbuff);
	encode_result = switchingEncodeDCBuffer(&dcbuff, &out_cfh);
    } else if (BDELTA_FORMAT == patch_format_id) {
        encode_result = bdeltaEncodeDCBuffer(&dcbuff, &out_cfh);
    }
    v1printf("encoding result was %ld\n", encode_result);
    DCBufferFree(&dcbuff);
    cclose(&ver_full);
    cclose(&out_cfh);
    close(src_fh);
    close(trg_fh);
    close(out_fh);
    return 0;
}

int 
cmp_ver_tar_ent_to_src_tar_ent(const void *te1, const void *te2)
{
    return strcmp( (*((tar_entry **)te1))->fullname + trg_common_len, (*((tar_entry **)te2))->fullname + src_common_len);
}

int 
cmp_tar_ents(const void *te1, const void *te2)
{
    return strcmp( 	(*((tar_entry **)te1))->fullname, 
    			(*((tar_entry **)te2))->fullname);
}


