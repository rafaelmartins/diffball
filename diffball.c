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
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "string-misc.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "tar.h"
#include "cfile.h"
#include "dcbuffer.h"
#include "hash.h"
#include "diff-algs.h"
#include "formats.h"
#include "defs.h"
#include "errors.h"
#include "options.h"

unsigned int global_verbosity = 0;
unsigned int global_use_md5 = 0;

unsigned int src_common_len=0, trg_common_len=0;

int cmp_ver_tar_ent_to_src_tar_ent(const void *te1, const void *te2);
int cmp_tar_ents(const void *te1, const void *te2);

int error;
unsigned int patch_compressor = 0;

int 
main(int argc, char **argv)
{
    int src_fh, trg_fh, out_fh;
    tar_entry *source = NULL;
    tar_entry **src_ptrs = NULL;
    tar_entry *target = NULL;
    tar_entry *tar_ptr = NULL;
    unsigned int patch_to_stdout = 0;
    char *patch_format = NULL;
    void *vptr;
    signed err;
    signed int ref_id, ver_id;
    unsigned long source_count, target_count;
    signed long encode_result=0;
    unsigned long x, patch_format_id;
    char src_common[512], trg_common[512], *p;  /* common dir's... */
    unsigned long match_count;
    long sample_rate = 0, seed_len = 0, hash_size = 0;
	
    cfile ref_full, ref_window, ver_window, ver_full, out_cfh;
    struct stat ref_stat, ver_stat;
    RefHash rhash_win;
    CommandBuffer dcbuff;

//    signed long optr;
    int optr;
    char  *src_file = NULL;
    char  *trg_file = NULL;
    char  *patch_name = NULL;

    static struct option long_opts[] = {
	STD_LONG_OPTIONS,
	DIFF_LONG_OPTIONS,
	FORMAT_LONG_OPTION("patch-format",'f'),
	END_LONG_OPTS
    };

    static struct usage_options help_opts[] = {
	STD_HELP_OPTIONS,
	DIFF_HELP_OPTIONS,
	FORMAT_HELP_OPTION("patch-format",'f', "specify the generated patches format"),
	USAGE_FLUFF("Diffball expects normally 3 args- the source file, the target file,\n"
	"and the name for the new patch.  If it's told to output to stdout, it will- in which\n"
	"case only 2 non-options arguements are allowed.\n"
	"Example usage: diffball linux-2.6.8.tar linux-2.6.9.tar linux-2.6.8-2.6.9.patch"),
   	END_HELP_OPTS
    };

    #define DUMP_USAGE(exit_code)  \
	print_usage("diffball", "src_file trg_file [patch_file|or to stdout]", help_opts, exit_code)
    char short_opts[] = STD_SHORT_OPTIONS DIFF_SHORT_OPTIONS "f:";
    
    while((optr = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
	switch(optr) {
	case 'f':
	    patch_format = optarg;		break;
	case OSAMPLE:
	    sample_rate = atol(optarg);
	    if(sample_rate == 0 || sample_rate > MAX_SAMPLE_RATE) DUMP_USAGE(EXIT_USAGE);
	    break;
	case OHASH:
	    hash_size = atol(optarg);
	    if(hash_size == 0 || hash_size > MAX_HASH_SIZE) DUMP_USAGE(EXIT_USAGE);
	    break;
	case OSEED:
	    seed_len = atol(optarg);
	    if(seed_len == 0 || seed_len > MAX_SEED_LEN) DUMP_USAGE(EXIT_USAGE);
	    break;
	case OVERSION:
	    print_version("diffball");		exit(0);
	case OVERBOSE:
	    global_verbosity++;	    		break;
	case OSTDOUT:
	    patch_to_stdout = 1;		break;
	case OUSAGE:
	case OHELP:
	    DUMP_USAGE(0);			break;
/*	case OBZIP2:
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
	    break;*/
	default:
	    v0printf("invalid arg- %s\n", argv[optind]);
	    DUMP_USAGE(EXIT_USAGE);
	}
    }
    if( ((src_file = (char *)get_next_arg(argc, argv)) == NULL) ||
	(stat(src_file, &ref_stat))) {
	if(src_file) {
	    v0printf("%s not found!\n", src_file);
	    exit(EXIT_USAGE);
	}
	DUMP_USAGE(EXIT_USAGE);
    }
    if( ((trg_file=(char *)get_next_arg(argc, argv)) == NULL) ||
	(stat(trg_file, &ver_stat)) ) {
	if (trg_file) {
	    v0printf("%s not found!\n", trg_file);
	    exit(EXIT_USAGE);
	}
	DUMP_USAGE(EXIT_USAGE);
    }
    if(patch_format==NULL) {
	patch_format_id = DEFAULT_PATCH_ID;
    } else {
	patch_format_id = check_for_format(patch_format, strlen(patch_format));
	if(patch_format_id==0) {
	    v0printf( "Unknown format '%s'\n", patch_format);
	    exit(EXIT_USAGE);
	}
    }
    if(patch_to_stdout != 0) {
	out_fh = 1;
    } else {
	if((patch_name = (char *)get_next_arg(argc, argv)) == NULL)
	    DUMP_USAGE(EXIT_USAGE);
	if((out_fh = open(patch_name, O_WRONLY | O_TRUNC | O_CREAT, 0644))==-1) {
	    v0printf( "error creating patch file (open failed)\n");
	    exit(1);
	}
    }
    if(NULL != get_next_arg(argc, argv)) {
	DUMP_USAGE(EXIT_USAGE);
    }
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

    err = DCB_llm_init(&dcbuff, 4096, (unsigned long)ref_stat.st_size, 
	(unsigned long)ver_stat.st_size) ||
	DCB_llm_init_buff(&dcbuff, 4096);

    check_return2(err,"DCBufferInit");

    v1printf("looking for matching filenames in the archives...\n");

    ver_id = DCB_REGISTER_ADD_SRC(&dcbuff, &ver_full, NULL, 0);
    if(ver_id < 0) {
        check_return(ver_id, "DCB_REGISTER_ADD_SRC", "failed to register file handle");
    }

    ref_id = DCB_REGISTER_COPY_SRC(&dcbuff, &ref_full, NULL, 0);
    if(ref_id < 0) {
        check_return(ref_id, "DCB_REGISTER_COPY_SRC", "failed to register file handle");
    }

    for(x=0; x< target_count; x++) {
	v1printf("processing %lu of %lu\n", x + 1, target_count);
	tar_ptr = &target[x];
        vptr = bsearch(&tar_ptr, src_ptrs, 
	    source_count, sizeof(tar_entry **), cmp_ver_tar_ent_to_src_tar_ent);
        if(vptr == NULL) {
	    v1printf("didn't find a match for %.255s, skipping\n", 
		target[x].fullname);
        } else {
            tar_ptr = (tar_entry *)*((tar_entry **)vptr);
            v1printf("found match between %.255s and %.255s\n", target[x].fullname,
		tar_ptr->fullname);
	    v2printf("differencing src(%lu:%lu) against trg(%lu:%lu)\n",
		tar_ptr->start, tar_ptr->end, target[x].start, target[x].end);

	    copen_child_cfh(&ver_window, &ver_full, target[x].start, target[x].end,
            	NO_COMPRESSOR, CFILE_RONLY | CFILE_BUFFER_ALL);

            copen_child_cfh(&ref_window, &ref_full, tar_ptr->start, tar_ptr->end,
        	NO_COMPRESSOR, CFILE_RONLY | CFILE_BUFFER_ALL);
            
            match_count++;
	    err=rh_bucket_hash_init(&rhash_win, &ref_window, 24, 1, cfile_len(&ref_window));
//            err=init_RefHash(&rhash_win, &ref_window, 24, 1, 
//		cfile_len(&ref_window), RH_BUCKET_HASH);
	    check_return2(err,"init_RefHash");
	    err=RHash_insert_block(&rhash_win, &ref_window, 0, 
		cfile_len(&ref_window));
	    check_return2(err,"RHash_insert_block");
	    err=RHash_cleanse(&rhash_win);
	    check_return2(err,"RHash_cleanse");
	    print_RefHash_stats(&rhash_win);
            err=OneHalfPassCorrecting(&dcbuff, &rhash_win, ref_id, &ver_window, ver_id);
            
            if(err) {
            	/* not a graceful exit I realize... */
            	v0printf("OneHalfPassCorrecting returned an error process file %.255s and %.255s\n", 
            	    target[x].fullname, tar_ptr->fullname);
            	v0printf("Quite likely this is a bug in diffball; error's should not occur at this point, exempting out of memory errors\n");
            	v0printf("please contact the author so this can be resolved.\n");
		check_return2(err,"OneHalfPassCorrecting");
            }
//	    MultiPassAlg(&dcbuff, &ref_window, &ver_window, hash_size);
            err=free_RefHash(&rhash_win);
            check_return(err,"free_RefHash","This shouldn't be happening...");
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
    err=MultiPassAlg(&dcbuff, &ref_full, ref_id, &ver_full, ver_id, hash_size);
    check_return(err, "MultiPassAlg", "final multipass run failed");
    err=DCB_insert(&dcbuff);
    check_return2(err, "DCB_insert");
    cclose(&ref_full);

    copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY | CFILE_OPEN_FH);
    v1printf("outputing patch...\n");
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


