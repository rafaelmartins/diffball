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
#include "gdiff.h"
#include "switching.h"
#include "bdiff.h"
#include "bdelta.h"
#include "primes.h"
#include "defs.h"
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

unsigned int verbosity = 0;

int cmp_tar_entries(const void *te1, const void *te2);

unsigned int src_common_len=0, trg_common_len=0;

int main(int argc, char **argv)
{
    int src_fh, trg_fh, out_fh;
    unsigned int offset_type;
    struct tar_entry **source, **target, *tar_ptr;
    void *vptr;
    unsigned char source_md5[32], target_md5[32];
    unsigned long source_count, target_count;
    unsigned long x;
    char src_common[512], trg_common[512], *p;  /* common dir's... */
    //unsigned int src_common_len=0, trg_common_len=0;
    unsigned long match_count;
    /*probably should convert these arrays to something more compact, use bit masking. */
    unsigned char *source_matches, *target_matches;
	
	cfile ref_full, ref_window, ver_window, ver_full, out_cfh;
	struct stat ref_stat, ver_stat;
	RefHash rhash_full, rhash_win;
	CommandBuffer dcbuff;


    /*this will require a rewrite at some point to allow for options*/
    if (argc<4) {
		printf("Sorry, need three files here bub.  Source, Target, file-to-save-the-patch-in\n");
		exit(EXIT_FAILURE);
    }
    if ((src_fh = open(argv[1], O_RDONLY,0)) == -1) {
		printf("Couldn't open %s, does it exist?\n", argv[1]);
		exit(EXIT_FAILURE);
    } else if(stat(argv[1], &ref_stat)) {
    	printf("Couldn't stat %s...\n", argv[1]);
    	exit(EXIT_FAILURE);
    }
    if ((trg_fh = open(argv[2], O_RDONLY,0)) == -1) {
		printf("Couldn't open %s, does it exist?\n", argv[2]);
		exit(EXIT_FAILURE);
    } else if(stat(argv[2], &ver_stat)) {
    	printf("Couldn't stat %s...\n", argv[2]);
    	exit(EXIT_FAILURE);
    }
    if ((out_fh = open(argv[3], O_WRONLY | O_TRUNC | O_CREAT, 0644)) == -1) {
    	printf("couldn't create/truncate patch file %s.\n", argv[3]);
    	exit(EXIT_FAILURE);
    }
    copen(&out_cfh, out_fh, 0, 0, NO_COMPRESSOR, CFILE_WONLY);
    source = read_fh_to_tar_entry(src_fh, &source_count, source_md5);
    printf("source file md5sum=%.32s, count(%lu)\n", source_md5, source_count);
    target = read_fh_to_tar_entry(trg_fh, &target_count, target_md5);
    printf("target file md5sum=%.32s, count(%lu)\n", target_md5, target_count);
    /*for(x=0; x < source_count; x++) {
    	printf("have file %s\n", source[x]->working_name);
    }
    printf("count(%lu)\n", source_count);
    exit(0);
    */
    /* this next one is moreso for bsearch's, but it's prob useful for the common-prefix alg too */
    
    printf("qsorting\n");
    qsort((struct tar_entry **)source, source_count, sizeof(struct tar_entry *), cmp_tar_entries);
    printf("qsort done\n");
    
    /* alg to basically figure out the common dir prefix... eg, if everything is in dir 
    	debianutils-1.16.3*/
    /*note, we want the slash, hence +1 */
    src_common_len=(char *)rindex(
        (const char *)source[0]->fullname, '/') - (char *)source[0]->fullname+1;
    strncpy((char *)src_common, (char *)source[0]->fullname,src_common_len);
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
    printf("final src_common='%.*s'\n", src_common_len, src_common);
    trg_common_len=(char *)rindex(
        (const char *)target[0]->fullname, '/') - (char *)target[0]->fullname+1;
    strncpy((char *)trg_common, (char *)target[0]->fullname,trg_common_len);
    trg_common[trg_common_len] = '\0';  /* null delimit it */
    for (x=0; x < target_count; x++) {
        if (strncmp((const char *)trg_common, (const char *)target[x]->fullname, trg_common_len) !=0) {
            printf("found a breaker(%s) at pos(%lu), loc(%lu)\n", target[x]->fullname, x, target[x]->file_loc);
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
    printf("final trg_common='%.*s'\n", trg_common_len, trg_common);

    for (x=0; x < source_count; x++) {
        source[x]->working_name += src_common_len;
		source[x]->working_len -=  src_common_len;
//        source[x]->fullname_ptr= (char *)source[x]->fullname + src_common_len;
//		source[x]->fullname_ptr_len = source[x]->fullname_len - src_common_len;
    }
    
    for (x=0; x < target_count; x++) {
		target[x]->working_name += trg_common_len;
		target[x]->working_len -= trg_common_len;
//        target[x]->fullname_ptr = (char *)target[x]->fullname + trg_common_len;
//		target[x]->fullname_ptr_len = target[x]->fullname_len - trg_common_len;
    }

    /* the following is for identifying changed files. */
    /*if((source_matches = (char*)malloc(source_count))==NULL) {
		perror("couldn't alloc needed memory, dieing.\n");
		exit(EXIT_FAILURE);
    }
    if((target_matches = (char*)malloc(target_count))==NULL) {
		perror("couldn't alloc needed memory, dieing.\n");
		exit(EXIT_FAILURE);
    }
    match_count=0;*/
    //for(x=0; x < target_count; x++)
	//	target_matches[x] = '0';
    
    copen(&ref_full, src_fh, 0, ref_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    DCBufferInit(&dcbuff, 20000000, (unsigned long)ref_stat.st_size, 
	(unsigned long)ver_stat.st_size);
    init_RefHash(&rhash_full, &ref_full, 16, 1, cfile_len(&ref_full)/1);
    v1printf("looking for matching filenames in the archives...\n");
    for(x=0; x< target_count; x++) {
        //entry=source[x];
	//printf("checking '%s'\n", source[x]->fullname);
	copen(&ver_window, trg_fh, (512 * target[x]->file_loc),
        	(512 * target[x]->file_loc) + 512 + (target[x]->size==0 ? 0 :
        		target[x]->size + 512 - (target[x]->size % 512==0 ? 512 : 
        		target[x]->size % 512)),
        		NO_COMPRESSOR, CFILE_RONLY | CFILE_BUFFER_ALL);
        vptr = bsearch((const void **)&target[x], (const void **)source, source_count,
            sizeof(struct tar_entry **), cmp_tar_entries);
        if(vptr == NULL) {
        	v1printf("didn't find a match for %.255s\n", target[x]->fullname);
		v2printf("target loc(%lu:%lu)\n",
        		(512 * target[x]->file_loc), 
        		(512 * target[x]->file_loc) + 512 +(target[x]->size==0 ? 0 : 
        			target[x]->size + 512 - (target[x]->size % 512==0 ? 512 : 
        			target[x]->size % 512) ));
        	v2printf("file_loc(%lu), size(%lu)\n", target[x]->file_loc,
        			target[x]->size);
        	OneHalfPassCorrecting(&dcbuff, &rhash_full, &ver_window);
	    	//_matches[x] = '0';
	    	//printf("couldn't match '%.255s'\n",
	    	//	target[x]->fullname_ptr + trg_common_len);
            //printf("'%s' not found!\n", source[x]->fullname_ptr);
        } else {
        	tar_ptr = (struct tar_entry *)*((struct tar_entry **)vptr);
        	v1printf("found match between %.255s and %.255s\n", target[x]->fullname,
        		tar_ptr->fullname);
        	v2printf("differencing src(%lu:%lu) against trg(%lu:%lu)\n",
        		(512 * tar_ptr->file_loc), 
        		(512 * tar_ptr->file_loc) + 512 + (tar_ptr->size==0 ? 0 :
        			tar_ptr->size + 512 - (tar_ptr->size % 512==0 ? 512: 
        			tar_ptr->size % 512)),
        		(512 * target[x]->file_loc), 
        		(512 * target[x]->file_loc) + 512 +(target[x]->size==0 ? 0 : 
        			target[x]->size + 512 - (target[x]->size % 512==0 ? 512 : 
        			target[x]->size % 512) ));
        		v2printf("file_loc(%lu), size(%lu)\n", target[x]->file_loc,
        			target[x]->size);
        	match_count++;
        	copen(&ref_window, src_fh, (512 * tar_ptr->file_loc), 
        		(512 * tar_ptr->file_loc) + 512 + (tar_ptr->size==0 ? 0 :
        			tar_ptr->size + 512 - (tar_ptr->size % 512==0 ? 512 : 
        			tar_ptr->size % 512)),
        			NO_COMPRESSOR, CFILE_RONLY | CFILE_BUFFER_ALL);
        	init_RefHash(&rhash_win, &ref_window, 16, 1, cfile_len(&ref_window));
        	OneHalfPassCorrecting(&dcbuff, &rhash_win, &ver_window);
        	free_RefHash(&rhash_win);
		cclose(&ref_window);
	    	/*printf("matched  '%s'\n", target[(struct tar_entry **)vptr - target]->fullname);
		    printf("correct  '%s'\n\n", ((*(struct tar_entry **)vptr)->fullname));*/
		    //source_matches[x] = '1';
		    /* note this works since the type cast makes ptr arithmetic already deal w/ ptr size. */
		    //target_matches[(struct tar_entry **)vptr - target] = '1';
		    //target_matches[(vptr - target)/(sizeof(struct tar_entry **))] = '1';
		    //target_matches[((struct tar_entry *)
            //printf("'%s' found!\n", source[x]->fullname_ptr);
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
    	printf("must be a null padded tarball. processing the remainder.\n");
    	DCBufferAddCmd(&dcbuff, DC_ADD, x, ver_stat.st_size - x);
    }
//    printf("matched(%lu), couldn't match(%lu) of entry count(%lu).\n", 
//	match_count, target_count -match_count, target_count);
        
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
//    printf("collapsing adds.\n");
//    DCBufferCollapseAdds(&dcbuff);
    printf("outputing patch...\n");
    copen(&ver_full, trg_fh, 0, ver_stat.st_size, NO_COMPRESSOR, CFILE_RONLY);
    offset_type= ENCODING_OFFSET_DC_POS;
//    offset_type= ENCODING_OFFSET_START;
//    rawEncodeDCBuffer(&dcbuff, offset_type, &ver_full, &out_cfh);
//    switchingEncodeDCBuffer(&dcbuff, offset_type, &ver_full, &out_cfh);
    gdiffEncodeDCBuffer(&dcbuff, offset_type, &ver_full, &out_cfh);
//    bdiffEncodeDCBuffer(&dcbuff, &ver_full, &out_cfh);
//    bdeltaEncodeDCBuffer(&dcbuff, &ver_full, &out_cfh);
    DCBufferFree(&dcbuff);
    cclose(&ver_full);
    cclose(&out_cfh);
    close(src_fh);
    close(trg_fh);
    return 0;
}

int cmp_tar_entries(const void *te1, const void *te2)
{
    //printf("in cmp_tar_entries\n");
    struct tar_entry *p1=*((struct tar_entry **)te1);
    struct tar_entry *p2=*((struct tar_entry **)te2);
    return(strcmp((char *)(p1->working_name), 
    	(char *)(p2->working_name)));
}


