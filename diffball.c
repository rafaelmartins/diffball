#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <search.h>
#include <fcntl.h>
#include "tar.h"
#include "data-structs.h"

int cmp_tar_entries(const void *te1, const void *te2);
int command_pipes(const char *command, const char *args, int *pipes);

int main(int argc, char **argv)
{
    int src_fh;
    int trg_fh;
    struct tar_entry **source, **target, *tar_ptr;
    void *vptr;
    unsigned char source_md5[32], target_md5[32];
    unsigned long source_count, target_count;
    unsigned long x;
    char src_common[256], trg_common[256];  /* common dir's... */
    unsigned int src_common_len=0, trg_common_len=0;
    /*probably should convert these arrays to something more compact, use bit masking. */
    unsigned char *source_matches, *target_matches;
    
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
        to the start of fullname. that and look for common info for each entry. */
        /*note for harring.  deref fullname, add common_len, then assign to ptr after casting */
    
    for (x=0; x < source_count; x++) {
        source[x]->fullname_ptr= (char *)source[x]->fullname + src_common_len;
    }
    
    struct long_dllist *init_long_dllist(unsigned long int value) {
	struct long_dllist *em;
	if((em = (struct long_dllist *)malloc(sizeof(struct str_dllist)))==NULL){
	    perror("couldn't alloc needed memory...\n");
	    exit(EXIT_FAILURE);
	}
	em->data = value;
	em->count = 0;
	em->prev = em->next = NULL;
	printf("creating new lldl node, value(%lu)\n", value);
	return em;
    }
    struct str_dllist *init_str_dllist(char *string, int len) {
	struct str_dllist *em;
	if((em = (struct str_dllist *)malloc(sizeof(struct str_dllist)))==NULL){
	    perror("couldn't alloc needed memory...\n");
	    exit(EXIT_FAILURE);
	} else if((em->data = (char *)malloc(len+1))==NULL) {
	    perror("couldn't alloc needed memory...\n");
	    exit(EXIT_FAILURE);
	}
	strncpy((char *)em->data, (char *)string, len);
	em->data[len] = '\0';
	em->count=0;
	em->prev = em->next = NULL;
	return em;
    }

    printf("initing struct's for checking for common longs...\n");
    struct long_dllist *trg_mode_ll, trg_uid_ll, trg_gid_ll, trg_devmajor_ll, trg_devminor_ll, *ldll_ptr;
    trg_mode_ll = init_long_dllist(target[0]->mode);
    trg_uid_ll = *init_long_dllist(target[0]->uid);
    trg_gid_ll = *init_long_dllist(target[0]->gid);
    trg_devmajor_ll = *init_long_dllist(target[0]->devmajor);
    trg_devminor_ll = *init_long_dllist(target[0]->devminor);
    printf("initing struct's for checking for common strs...\n");
    struct str_dllist trg_uname_ll, trg_gname_ll, trg_magic_ll, trg_version_ll, trg_mtime_ll, *sdll_ptr;
    trg_uname_ll = *init_str_dllist(target[0]->uname, strnlen(target[0]->uname, TAR_UNAME_LEN));
    printf("finished 1\n");
    trg_gname_ll = *init_str_dllist(target[0]->gname, strnlen(target[0]->gname, TAR_GNAME_LEN));
    trg_magic_ll = *init_str_dllist(target[0]->magic, strnlen(target[0]->magic, TAR_MAGIC_LEN));
    trg_version_ll = *init_str_dllist(target[0]->version, strnlen(target[0]->version, TAR_VERSION_LEN));
    trg_mtime_ll = *init_str_dllist(target[0]->mtime, strnlen(target[0]->mtime, TAR_MTIME_LEN));
    printf("inited.\n");
    
    void update_long_dllist(struct long_dllist **em, unsigned long int value) {
	struct long_dllist *ptr, *tmp;
	int x = 0;
	printf("\nupdating\n");
	for (ptr = *em; ptr != NULL; ptr = (struct long_dllist *)ptr->next, x++) {
	    printf("examining node(%u)\n", x);
	    if(ptr->data == value) {
		printf("match ptr(%lu), data(%lu), current count(%lu)\n", ptr, ptr->data, ptr->count);
		ptr->count += 1;
		while (ptr->prev != NULL
		   && ptr->count > ptr->prev->count) {
		    /* I'm sure this could be wrote better. don't feel like screwing with it though. */
		    tmp = ptr->prev;
		    printf("moving node, p.count(%u), c.count(%u)\n",ptr->prev->count, ptr->count);
		    printf("  p.p(%lu), p.n(%lu); n.p(%lu), n.n(%lu)\n", tmp->prev, tmp->next, ptr->prev, ptr->next);
		    ptr->prev = tmp->prev;
		    tmp->next = ptr->next;
		    if (tmp->prev != NULL)
			tmp->prev->next = ptr;
		    tmp->prev = ptr;
		    if (ptr->next != NULL)
			ptr->next->prev = tmp;
		    ptr->next = tmp;
		}
		if(ptr->prev == NULL)
		    *em = ptr;
		break;
	    } else if (ptr->next == NULL) {
		printf("adding llist value(%lu)\n", value);
		ptr->next = init_long_dllist(value);
		ptr->next->prev = ptr;
		ptr->next->count += 1;
		printf("  c.p(%lu), c(%lu), c.n(%lu), n.p(%lu), n.n(%lu)\n",ptr->prev, ptr, ptr->next, ptr->next->prev, ptr->next->next);
		break;
	    }
	}
    }
		    
    for (x=0; x < target_count; x++) {
        target[x]->fullname_ptr = (char *)target[x]->fullname + trg_common_len;
	update_long_dllist(&trg_mode_ll, target[x]->mode);
	/*update_long_dllist(&trg_uid_ll, target[x]->uid);
	update_long_dllist(&trg_gid_ll, target[x]->gid);
	update_long_dllist(&trg_devminor_ll, target[x]->devminor);
	update_long_dllist(&trg_devmajor_ll, target[x]->devminor);*/
    }
    printf("checking ldll\n");
    for(ldll_ptr = trg_mode_ll; ldll_ptr != NULL; ldll_ptr = ldll_ptr->next) 
	printf("value=%lu, count=%lu\n", ldll_ptr->data, ldll_ptr->count);
    printf("qsorting\n");
    /* this next one is moreso for bsearch's, but it's prob useful for the common-prefix alg too */
    qsort((struct tar_entry **)target, target_count, sizeof(struct tar_entry *), cmp_tar_entries);
    printf("qsort done\n");

    /* the following is for identifying changed files. */
    if((source_matches = (char*)malloc(source_count))==NULL) {
	perror("couldn't alloc needed memory, dieing.\n");
	exit(EXIT_FAILURE);
    }
    if((target_matches = (char*)malloc(target_count))==NULL) {
	perror("couldn't alloc needed memory, dieing.\n");
	exit(EXIT_FAILURE);
    }
    for(x=0; x < target_count; x++)
	target_matches[x] = '0';
    
    printf("looking for matching filenames in the archives...\n");
    for(x=0; x< source_count; x++) {
        //entry=source[x];
	//printf("checking '%s'\n", source[x]->fullname);
        vptr = bsearch((const void **)&source[x], (const void **)target, target_count,
            sizeof(struct tar_entry **), cmp_tar_entries);
        if(vptr == NULL) {
	    source_matches[x] = '0';
            //printf("'%s' not found!\n", source[x]->fullname_ptr);
        } else {
	    /*printf("matched  '%s'\n", target[(struct tar_entry **)vptr - target]->fullname);
	    printf("correct  '%s'\n\n", ((*(struct tar_entry **)vptr)->fullname));*/
	    source_matches[x] = '1';
	    /* note this works since the type cast makes ptr arithmetic already deal w/ ptr size. */
	    target_matches[(struct tar_entry **)vptr - target] = '1';
	    //target_matches[(vptr - target)/(sizeof(struct tar_entry **))] = '1';
	    //target_matches[((struct tar_entry *)
            //printf("'%s' found!\n", source[x]->fullname_ptr);
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

