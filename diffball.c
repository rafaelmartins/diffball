#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <search.h>
#include <string.h>
#include <fcntl.h>
#include "tar.h"
#include "data-structs.h"

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
int cmp_tar_entries(const void *te1, const void *te2);
int command_pipes(const char *command, const char *args, int *pipes);

struct long_dllist *init_long_dllist(unsigned long int value) {
    struct long_dllist *em;
    if((em = (struct long_dllist *)malloc(sizeof(struct str_dllist)))==NULL){
        perror("couldn't alloc needed memory...\n");
        exit(EXIT_FAILURE);
    }
    em->data = value;
    em->count = 0;
    em->prev = em->next = NULL;
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
    em->len=len;
    em->prev = em->next = NULL;
    return em;
}

void update_long_dllist(struct long_dllist **em, unsigned long int value) {
    struct long_dllist *ptr, *tmp;
    int x = 0;
    //printf("\nupdating\n");
    for (ptr = *em; ptr != NULL; ptr = (struct long_dllist *)ptr->next, x++) {
        //printf("examining node(%u)\n", x);
        if(ptr->data == value) {
    	//printf("match ptr(%lu), data(%lu), current count(%lu)\n", ptr, ptr->data, ptr->count);
    	ptr->count += 1;
    	while (ptr->prev != NULL
    	   && ptr->count > ptr->prev->count) {
    	    /* I'm sure this could be wrote better. don't feel like screwing with it though. */
    	    tmp = ptr->prev;
    	    //printf("moving node, p.count(%u), c.count(%u)\n",ptr->prev->count, ptr->count);
    	    //printf("  p.p(%lu), p.n(%lu); n.p(%lu), n.n(%lu)\n", tmp->prev, tmp->next, ptr->prev, ptr->next);
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
    	//printf("adding llist value(%lu)\n", value);
    	ptr->next = init_long_dllist(value);
    	ptr->next->prev = ptr;
    	ptr->next->count += 1;
    	//printf("  c.p(%lu), c(%lu), c.n(%lu), n.p(%lu), n.n(%lu)\n",ptr->prev, ptr, ptr->next, ptr->next->prev, ptr->next->next);
    	break;
        }
    }
}
    
void update_str_dllist(struct str_dllist **em, char *value, int len) {
    struct str_dllist *ptr, *tmp;
    int x = 0;
    //printf("\nupdating\n");
    for (ptr = *em; ptr != NULL; ptr = (struct str_dllist *)ptr->next, x++) {
        //printf("examining node('%u'), '%s'=='%s'\n", x, ptr->data, value);
        /* this next statement likely could use some tweaking. */
        if(len == ptr->len && strncmp(ptr->data, value,len) == 0) {
    	//printf("match ptr(%lu), data('%s'), current count(%lu)\n", ptr, ptr->data, ptr->count);
    	ptr->count += 1;
    	while (ptr->prev != NULL
    	   && ptr->count > ptr->prev->count) {
    	    /* I'm sure this could be wrote better. don't feel like screwing with it though. */
    	    tmp = ptr->prev;
    	    //printf("moving node, p.count(%u), c.count(%u)\n",ptr->prev->count, ptr->count);
    	    //printf("  p.p(%lu), p.n(%lu); n.p(%lu), n.n(%lu)\n", tmp->prev, tmp->next, ptr->prev, ptr->next);
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
    	//printf("adding llist value('%s')\n", value);
    	ptr->next = init_str_dllist(value, len);
    	ptr->next->prev = ptr;
    	ptr->next->count += 1;
    	//printf("  new node data('%s')\n", ptr->data);
    	//printf("  c.p(%lu), c(%lu), c.n(%lu), n.p(%lu), n.n(%lu)\n",ptr->prev, ptr, ptr->next, ptr->next->prev, ptr->next->next);
    	break;
        }
    }
}



int main(int argc, char **argv)
{
    int src_fh;
    int trg_fh;
    struct tar_entry **source, **target, *tar_ptr;
    void *vptr;
    unsigned char source_md5[32], target_md5[32];
    unsigned long source_count, target_count, halfway;
    unsigned long x;
    char src_common[256], trg_common[256], *p;  /* common dir's... */
    unsigned int src_common_len=0, trg_common_len=0;
    /*probably should convert these arrays to something more compact, use bit masking. */
    unsigned char *source_matches, *target_matches;
    struct long_dllist *trg_mode_ll, *trg_uid_ll, *trg_gid_ll, *trg_devmajor_ll, *trg_devminor_ll, *ldll_ptr;
    struct str_dllist *trg_uname_ll, *trg_gname_ll, *trg_magic_ll, *trg_version_ll, *trg_mtime_ll, *sdll_ptr;


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

    printf("initing struct's for checking for common longs...\n");
    trg_mode_ll = init_long_dllist(target[0]->mode);
    trg_uid_ll = init_long_dllist(target[0]->uid);
    trg_gid_ll = init_long_dllist(target[0]->gid);
    trg_devmajor_ll = init_long_dllist(target[0]->devmajor);
    trg_devminor_ll = init_long_dllist(target[0]->devminor);
    printf("initing struct's for checking for common strs...\n");
    trg_uname_ll = init_str_dllist(target[0]->uname, target[0]->uname_len);
    trg_gname_ll = init_str_dllist(target[0]->gname, target[0]->gname_len);
    trg_magic_ll = init_str_dllist(target[0]->magic, TAR_MAGIC_LEN);
    trg_version_ll = init_str_dllist(target[0]->version, TAR_VERSION_LEN);
    trg_mtime_ll = init_str_dllist(target[0]->mtime, TAR_MTIME_LEN);
    printf("inited.\n");
        
        /*perhaps this is a crappy method, but basically for the my sanity, just up the fullname ptr
         to remove the common-prefix.  wonder if string functions behave and don't go past the sp... */
        /* init the fullname_ptr to point to the char after the common-prefix dir.  if no prefix, points
        to the start of fullname. that and look for common info for each entry. */
        /*note for harring.  deref fullname, add common_len, then assign to ptr after casting */
    
    for (x=0; x < source_count; x++) {
        source[x]->fullname_ptr= (char *)source[x]->fullname + src_common_len;
	source[x]->fullname_ptr_len = source[x]->fullname_len - src_common_len;
    }
    
    for (x=0; x < target_count; x++) {
        target[x]->fullname_ptr = (char *)target[x]->fullname + trg_common_len;
	target[x]->fullname_ptr_len = target[x]->fullname_len - trg_common_len;
    }
    halfway = (target_count+1)/2; // give it a +1 for those odd numbers, ensure it's 50%
    for (x=0; x < target_count && halfway > trg_mode_ll->count; x++)
	update_long_dllist(&trg_mode_ll, target[x]->mode);
    for (x=0; x < target_count && halfway > trg_uid_ll->count; x++)
	update_long_dllist(&trg_uid_ll, target[x]->uid);
    for (x=0; x < target_count && halfway > trg_gid_ll->count; x++)
	update_long_dllist(&trg_gid_ll, target[x]->gid);
    for (x=0; x < target_count && halfway > trg_devminor_ll->count; x++)
	update_long_dllist(&trg_devminor_ll, target[x]->devminor);
    for (x=0; x < target_count && halfway > trg_devmajor_ll->count; x++)
	update_long_dllist(&trg_devmajor_ll, target[x]->devminor);
    for (x=0; x < target_count && halfway > trg_uname_ll->count; x++)
	update_str_dllist(&trg_uname_ll, target[x]->uname, target[x]->uname_len);
    for (x=0; x < target_count && halfway > trg_gname_ll->count; x++)
	update_str_dllist(&trg_gname_ll, target[x]->gname, target[x]->gname_len);
    for (x=0; x < target_count && halfway > trg_magic_ll->count; x++)
	update_str_dllist(&trg_magic_ll, target[x]->magic, TAR_MAGIC_LEN);
    for (x=0; x < target_count && halfway > trg_version_ll->count; x++)
	update_str_dllist(&trg_version_ll, target[x]->version, TAR_VERSION_LEN);
    for (x=0; x < target_count && halfway > trg_mtime_ll->count; x++)
	update_str_dllist(&trg_mtime_ll, target[x]->mtime, TAR_MTIME_LEN);
    
    /*printf("target has %lu files, halfway=%lu\n", target_count, halfway);
    printf("\ndumping mode\n");
    for(ldll_ptr = trg_mode_ll, x=0; ldll_ptr != NULL; ldll_ptr = ldll_ptr->next, x++) 
	printf("node (%lu), value=%lu, count=%lu\n", x, ldll_ptr->data, ldll_ptr->count);
    printf("mode total count=%lu\n", x);
    printf("\ndumping uid\n");
    for(ldll_ptr = trg_uid_ll, x=0; ldll_ptr != NULL; ldll_ptr = ldll_ptr->next, x++) 
	printf("value=%lu, count=%lu\n", ldll_ptr->data, ldll_ptr->count);
    printf("uid total count=%lu\n", x);
    printf("\ndumping gid\n");
    for(ldll_ptr = trg_gid_ll, x=0; ldll_ptr != NULL; ldll_ptr = ldll_ptr->next, x++) 
	printf("value=%lu, count=%lu\n", ldll_ptr->data, ldll_ptr->count);
    printf("gid total count=%lu\n", x);
    printf("\ndumping uname\n");
    for(sdll_ptr = trg_uname_ll, x=0; sdll_ptr != NULL; sdll_ptr = sdll_ptr->next, x++) 
	printf("value='%.*s', count=%lu\n", sdll_ptr->len, sdll_ptr->data, sdll_ptr->count);
    printf("uname total count=%lu\n", x);
    printf("\ndumping gname\n");
    for(sdll_ptr = trg_gname_ll, x=0; sdll_ptr != NULL; sdll_ptr = sdll_ptr->next, x++) 
	printf("value='%.*s', count=%lu\n", sdll_ptr->len, sdll_ptr->data, sdll_ptr->count);
    printf("gname total count=%lu\n", x);
    printf("\ndumping magic\n");
    for(sdll_ptr = trg_magic_ll, x=0; sdll_ptr != NULL; sdll_ptr = sdll_ptr->next, x++) 
	printf("value='%.*s', count=%lu\n", sdll_ptr->len, sdll_ptr->data, sdll_ptr->count);
    printf("magic total count=%lu\n", x);
    printf("\ndumping version\n");
    for(sdll_ptr = trg_version_ll, x=0; sdll_ptr != NULL; sdll_ptr = sdll_ptr->next, x++) 
	printf("value='%.*s', count=%lu\n", sdll_ptr->len, sdll_ptr->data, sdll_ptr->count);
    printf("version total count=%lu\n", x);
    printf("\ndumping mtime\n");
    for(sdll_ptr = trg_mtime_ll, x=0; sdll_ptr != NULL; sdll_ptr = sdll_ptr->next, x++) 
	printf("value='%.*s', count=%lu\n", sdll_ptr->len, sdll_ptr->data, sdll_ptr->count);
    printf("mtime total count=%lu\n\n", x);*/
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

