#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <search.h>
#include <fcntl.h>
#include "tar.h"

int cmp_tar_entries(const void *te1, const void *te2);
int command_pipes(const char *command, const char *args, int *pipes);

int main(int argc, char **argv)
{
    int src_fh;
    int trg_fh;
    struct tar_entry **source, **target, *tar_ptr;
    void *vptr;
    char source_md5[32], target_md5[32];
    unsigned long source_count, target_count;
    unsigned long x;
    char src_common[256], trg_common[256];  /* common dir's... */
    unsigned int src_common_len=0, trg_common_len=0;
    char *text = "debianutils-1.16.7/which.1";

/*    printf("sizeof struct tar_entry=%u, sizeof *tar_entry=%u, size of **tar_entry=%u\n",
        sizeof(struct tar_entry), sizeof(struct tar_entry *), sizeof(struct tar_entry**));
    printf("sizeof *char[6]=%u\n", sizeof(char *[6]));*/
    
    /*this will require a rewrite at some point to allow for options*/
    if (argc<4) {
	printf("Sorry, need three files here bub.  Source, Target, file-to-save-the-patch-in\n");
	exit(EXIT_FAILURE);
    }
    if ((src_fh = open(argv[1], O_RDONLY,0)) == -1) {
	printf("Couldn't open %s, does it exist?\n", argv[1]);
	exit(EXIT_FAILURE);
    }
    if ((trg_fh = open(argv[2], O_RDONLY,0)) == -1) {
	printf("Couldn't open %s, does it exist?\n", argv[2]);
	exit(EXIT_FAILURE);
    }
    source = read_fh_to_tar_entry(src_fh, &source_count, source_md5);
    printf("source file md5sum=%.32s\n", source_md5);
    target = read_fh_to_tar_entry(trg_fh, &target_count, target_md5);
    printf("target file md5sum=%.32s\n", target_md5);
    
    /* alg to basically figure out the common dir prefix... eg, if everything is in dir debianutils-1.16.3
       one thing I do wonder about... I make user of ptr algebra, do these methods hold for all architectures?
       ergo, do all arch's storage of a string have for ptr's string[x] < then string[x+1]? */
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
    printf("final src_common='%.255s'\n", src_common);
    trg_common_len=(char *)rindex(
        (const char *)target[0]->fullname, '/') - (char *)target[0]->fullname+1;
    strncpy((char *)trg_common, (char *)target[0]->fullname,trg_common_len);
    trg_common[trg_common_len] = '\0';  /* null delimit it */
    for (x=0; x < target_count; x++) {
        if (strncmp((const char *)trg_common, (const char *)target[x]->fullname, trg_common_len) !=0) {
            printf("found a breaker(%s) at pos(%lu), loc(%lu)\n", target[x]->fullname, x, target[x]->file_loc);
            char *p;
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
    printf("final trg_common='%.255s'\n", trg_common);
        /*perhaps this is a crappy method, but basically for the my sanity, just up the fullname ptr
         to remove the common-prefix.  wonder if string functions behave and don't go past the sp... */
        /* init the fullname_ptr to point to the char after the common-prefix dir.  if no prefix, points
        to the start of fullname. */
        /*note for harring.  deref fullname, add common_len, then assign to ptr after casting */
    for (x=0; x < source_count; x++)
        source[x]->fullname_ptr= (char *)source[x]->fullname + src_common_len;
    for (x=0; x < target_count; x++) 
        target[x]->fullname_ptr = (char *)target[x]->fullname + trg_common_len;
    //printf("testing something='%s'\n", (char *)source[2]->fullname_ptr);
    
    /* this next one is moreso for bsearch's, but it's prob useful for the common-prefix alg too */
    qsort((struct tar_entry **)target, target_count, sizeof(struct tar_entry *), cmp_tar_entries);
    printf("qsort done\n");
    for(x=0; x< source_count; x++) {
        //entry=source[x];
        vptr = bsearch((const void **)&source[x], (const void **)target, target_count,
            sizeof(struct tar_entry **), cmp_tar_entries);
        if(vptr == NULL) {
            printf("'%s' not found!\n", source[x]->fullname_ptr);
        } else {
            printf("'%s' found!\n", source[x]->fullname_ptr);
        }
    }


        
    /* cleanup */
    printf("freeing source: elements, ");
    for(x=0; x< source_count; x++) {
        tar_ptr=source[x];
        free(tar_ptr);
    }
    printf("array.\n");
    free(source);
    printf("freeing target: elements, ");
    for(x=0; x< target_count; x++) {
        tar_ptr=target[x];
        free(tar_ptr);
    }
    printf("array.\n");
    free(target);
    close(src_fh);
    close(trg_fh);

}

int cmp_tar_entries(const void *te1, const void *te2)
{
    //printf("in cmp_tar_entries\n");
    struct tar_entry *p1=*((struct tar_entry **)te1);
    struct tar_entry *p2=*((struct tar_entry **)te2);
    return(strncmp((char *)p1->fullname_ptr, (char *)p2->fullname_ptr, 255));
}

int command_pipes(const char *command, const char *args, int *ret_pipes)
{
    int parent_write[2];
    int child_write[2];
    int fork_result;
    if (pipe(parent_write)==0 && pipe(child_write)==0){
        fork_result=fork();
        switch(fork_result)
        {
        case -1:
            fprintf(stderr, "hmm. fork failure.  eh?\n");
            return -1;
        case 0:
            //child
            //close(0);
            dup2(parent_write[0],0);
            //close(parent_write[0]);
            close(parent_write[1]);
            close(parent_write[0]);
            //close(1);
            dup2(child_write[1],1);
            close(child_write[0]);
            close(child_write[1]);
            //close(child_write[1]);
            /* hokay. the child's input comes from parent, output goes to parent now. */
            /* now to do the execing */
            execlp(command, command, args, (char *)0);
            exit(2); /* if this is reached, there are problems. */
            break;
        default:
            close(parent_write[0]);
            ret_pipes[1]=parent_write[1];
            close(child_write[1]);
            ret_pipes[0]=child_write[0];
            break;
        }
    }
    return 0;
    
}

