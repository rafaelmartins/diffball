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
//#include <search.h>
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

int cmp_tar_entries(const void *te1, const void *te2);

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

int src_common_len=0, trg_common_len=0;

int main(int argc, char **argv)
{
    int src_fh, trg_fh, out_fh;
    struct tar_entry **source, **target, *tar_ptr;
    void *vptr;
    unsigned long source_count, target_count;
    unsigned long x, patch_format_id, encode_result;
    char src_common[512], trg_common[512], *p;  /* common dir's... */
    //unsigned int src_common_len=0, trg_common_len=0;
    unsigned long match_count;
    /*probably should convert these arrays to something more compact, 
	use bit masking. */
//    unsigned char *source_matches, *target_matches;
	
	cfile ref_full, ref_window, ver_window, ver_full, out_cfh;
	struct stat ref_stat, ver_stat;
	RefHash rhash_full, rhash_win;
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
	    fprintf(stderr, "Unknown format '%s'\n", patch_format);
	    exit(1);
	}
    }
    if(patch_to_stdout != 0) {
	out_fh = 0;
    } else {
	if((patch_name = (char *)poptGetArg(p_opt))==NULL)
	    usage(p_opt, 1, "Must specify a name for the patch file.", NULL);
	if((out_fh = open(patch_name, O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
	    fprintf(stderr, "error creating patch file (open failed)\n");
	    exit(1);
	}
    }
    if(NULL!=poptGetArgs(p_opt)) {
	usage(p_opt, 1, poptBadOption(p_opt, POPT_BADOPTION_NOALIAS),
	"unknown option");
    }
    poptFreeContext(p_opt);
    if(sample_rate==0) {
	/* implement a better assessment based on mem and such */
	sample_rate = 1;
    }
    if(hash_size==0) {
	/* implement a better assessment based on mem and such */
	hash_size = /*65536;//*/ ref_stat.st_size;
    }
    if(seed_len==0) {
	seed_len = DEFAULT_SEED_LEN;
    }
    v1printf("using patch format %lu\n", patch_format_id);
    v1printf("using seed_len(%lu), sample_rate(%lu), hash_size(%lu)\n", 
	seed_len, sample_rate, hash_size);
    v1printf("verbosity level(%u)\n", global_verbosity);
    if((src_fh = open(src_file, O_RDONLY,0)) == -1) {
	fprintf(stderr, "error opening source file '%s'\n", src_file);
	exit(1);
    }
    if((trg_fh = open(trg_file, O_RDONLY,0)) == -1) {
	fprintf(stderr, "error opening target file '%s'\n", trg_file);
	exit(1);
    }
    copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY | CFILE_OPEN_FH);
    source = read_fh_to_tar_entry(src_fh, &source_count);
    target = read_fh_to_tar_entry(trg_fh, &target_count);
    v1printf("source tarball's entry count=%lu\n", source_count);
    v1printf("target tarball's entry count=%lu\n", target_count);
    /* this next one is moreso for bsearch's, but it's prob useful for the common-prefix alg too */
    
    v1printf("qsorting\n");
    qsort((struct tar_entry **)source, source_count, sizeof(struct tar_entry *), cmp_tar_entries);
    v1printf("qsort done\n");
    
    /* alg to basically figure out the common dir prefix... eg, if everything is in dir 
    	debianutils-1.16.3*/
    /*note, we want the slash, hence +1 */
    p = rindex(source[0]->fullname, '/');
    if(p!=NULL) {
	src_common_len = ((char *)p - (char *)source[0]->fullname) + 1;
	strncpy((char *)src_common, (char *)source[0]->fullname, 
	    src_common_len);
    } else {
	src_common_len = source[0]->fullname_len -1;
	strncpy(src_common, source[0]->fullname, src_common_len);
    }
    src_common[src_common_len] = '\0';  /*null delimit it */

    for (x=0; x < source_count; x++) {
        if (strncmp((const char *)src_common, (const char *)source[x]->fullname, src_common_len) !=0) {
            char *p;
            /* null the / at src_common_len-1, and attempt rindex again. */
            src_common[src_common_len -1]='\0';
            if((p = rindex(src_common, '/'))==NULL){
                src_common_len=0;
                src_common[0]='\0'; /*no common dir prefix. damn. */
            } else {
                src_common_len= src_common - p + 1; /*include the / again... */
                src_common[src_common_len +1]='\0';
            }
        }
    }
    v1printf("final src_common='%.*s'\n", src_common_len, src_common);
    p = rindex(target[0]->fullname, '/');
    if(p!=NULL) {
	trg_common_len = ((char *)p - (char *)target[0]->fullname) + 1;
	strncpy((char *)trg_common, (char *)target[0]->fullname, 
	    trg_common_len);
    } else {
	trg_common_len = target[0]->fullname_len -1;
	strncpy(trg_common, target[0]->fullname, trg_common_len);
    }
    trg_common[trg_common_len] = '\0';  /* null delimit it */

    for (x=0; x < target_count; x++) {
        if (strncmp((const char *)trg_common, (const char *)target[x]->fullname, 
	    trg_common_len) !=0) {
            v1printf("found a breaker(%s) at pos(%lu), loc(%lu)\n", 
		target[x]->fullname, x, target[x]->file_loc);

            /* null the / at trg_common_len-1, and attempt rindex again. */
            trg_common[trg_common_len -1]='\0';
            if((p = rindex(trg_common, '/'))==NULL){
                trg_common_len=0;
                trg_common[0]='\0'; /*no common dir prefix. damn. */
            } else {
                trg_common_len= trg_common - p + 1; /*include the / again... */
                trg_common[trg_common_len +1]='\0';
            }
        }
    }
    v1printf("final trg_common='%.*s'\n", trg_common_len, trg_common);

    for (x=0; x < source_count; x++) {
        source[x]->working_name += src_common_len;
	source[x]->working_len -=  src_common_len;
    }
    
    for (x=0; x < target_count; x++) {
	target[x]->working_name += trg_common_len;
	target[x]->working_len -= trg_common_len;
    }

    copen(&ref_full, src_fh, 0, ref_stat.st_size, NO_COMPRESSOR, CFILE_RONLY |
	CFILE_OPEN_FH);
    DCBufferInit(&dcbuff, 4096, (unsigned long)ref_stat.st_size, 
	(unsigned long)ver_stat.st_size, DCBUFFER_FULL_TYPE);
    v1printf("initing fallback full reference hash\n");
    init_RefHash(&rhash_full, &ref_full, seed_len, sample_rate, 
	hash_size);
    print_RefHash_stats(&rhash_full);
    v1printf("looking for matching filenames in the archives...\n");
    for(x=0; x< target_count; x++) {
	v1printf("processing %lu of %lu\n", x + 1, target_count);
	copen(&ver_window, trg_fh, (512 * target[x]->file_loc),
            (512 * target[x]->file_loc) + 512 + (target[x]->size==0 ? 0 :
            target[x]->size + 512 - (target[x]->size % 512==0 ? 
	    512 : target[x]->size % 512)),
            NO_COMPRESSOR, CFILE_RONLY | CFILE_BUFFER_ALL);
        vptr = bsearch((const void **)&target[x], (const void **)source, 
	    source_count, sizeof(struct tar_entry **), cmp_tar_entries);
        if(vptr == NULL) {
	    v1printf("didn't find a match for %.255s\n", target[x]->fullname);
	    v2printf("target loc(%lu:%lu)\n", (512 * target[x]->file_loc), 
        	(512 * target[x]->file_loc) + 512 +(target[x]->size==0 ? 0 : 
        	target[x]->size + 512 - (target[x]->size % 512==0 ? 512 : 
        	target[x]->size % 512) ));
            v1printf("file_loc(%lu), size(%lu)\n", target[x]->file_loc,
        	target[x]->size);
            OneHalfPassCorrecting(&dcbuff, &rhash_full, &ver_window);
        } else {
            tar_ptr = (struct tar_entry *)*((struct tar_entry **)vptr);
            v1printf("found match between %.255s and %.255s\n", target[x]->fullname,
		tar_ptr->fullname);
	    v2printf("differencing src(%lu:%lu) against trg(%lu:%lu)\n",
        	(512 * tar_ptr->file_loc), (512 * tar_ptr->file_loc) + 512 + 
		(tar_ptr->size==0 ? 0 :	tar_ptr->size + 512 - 
		(tar_ptr->size % 512==0 ? 512: 	tar_ptr->size % 512)),
		(512 * target[x]->file_loc), (512 * target[x]->file_loc) + 
		512 +(target[x]->size==0 ? 0 : target[x]->size + 512 - 
		(target[x]->size % 512==0 ? 512 : target[x]->size % 512) ));
	    v2printf("file_loc(%lu), size(%lu)\n", target[x]->file_loc,
        	target[x]->size);
            match_count++;
            copen(&ref_window, src_fh, (512 * tar_ptr->file_loc), 
        	(512 * tar_ptr->file_loc) + 512 + (tar_ptr->size==0 ? 0 :
        	tar_ptr->size + 512 - (tar_ptr->size % 512==0 ? 512 : 
        	tar_ptr->size % 512)),
        	NO_COMPRESSOR, CFILE_RONLY | CFILE_BUFFER_ALL);
            init_RefHash(&rhash_win, &ref_window, 16, 1, cfile_len(&ref_window));
	    v1printf("reference window stats:\n");
	    print_RefHash_stats(&rhash_win);
            OneHalfPassCorrecting(&dcbuff, &rhash_win, &ver_window);
            free_RefHash(&rhash_win);
	    cclose(&ref_window);
        }
        cclose(&ver_window);
    }
    free_RefHash(&rhash_full);
    cclose(&ref_full);
    x= (target[target_count -1]->file_loc * 512) + 512 + 
    	(target[target_count -1]->size==0 ? 0 : target[target_count -1]->size + 
    		512 - ( target[target_count -1]->size % 512==0 ? 512 :
    			target[target_count -1]->size % 512));
    	if(x!= ver_stat.st_size) {
    	v1printf("must be a null padded tarball. processing the remainder.\n");
	DCB_add_add(&dcbuff, x, ver_stat.st_size -x );
    }
        
    /* cleanup */
    for(x=0; x< source_count; x++) {
        free(source[x]->fullname);
        free(source[x]);
    }
    for(x=0; x< target_count; x++) {
	free(target[x]->fullname);
	free(target[x]);
    }
    free(target);
    free(source);
    copen(&ver_full, trg_fh, 0, ver_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    v1printf("outputing patch...\n");
    v1printf("there were %lu commands\n", dcbuff.DCB.full.buffer_count);
    if(GDIFF4_FORMAT == patch_format_id) { 
        encode_result = gdiff4EncodeDCBuffer(&dcbuff, &ver_full, &out_cfh);
    } else if(GDIFF5_FORMAT == patch_format_id) {
        encode_result = gdiff5EncodeDCBuffer(&dcbuff, &ver_full, &out_cfh);
    } else if(BDIFF_FORMAT == patch_format_id) {
        encode_result = bdiffEncodeDCBuffer(&dcbuff, &ver_full, &out_cfh);
    } else if(SWITCHING_FORMAT == patch_format_id) {
	DCBufferCollapseAdds(&dcbuff);
	encode_result = switchingEncodeDCBuffer(&dcbuff, &ver_full, &out_cfh);
    } else if (BDELTA_FORMAT == patch_format_id) {
        encode_result = bdeltaEncodeDCBuffer(&dcbuff, &ver_full, &out_cfh);
    }
    DCBufferFree(&dcbuff);
    cclose(&ver_full);
    cclose(&out_cfh);
    close(src_fh);
    close(trg_fh);
    return 0;
}

int cmp_tar_entries(const void *te1, const void *te2)
{
    struct tar_entry *p1=*((struct tar_entry **)te1);
    struct tar_entry *p2=*((struct tar_entry **)te2);
    return(strcmp((char *)(p1->working_name), 
    	(char *)(p2->working_name)));
}


