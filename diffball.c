#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <search.h>
#include <fcntl.h>


#include "tar.c"

struct tar_entry *read_fh_to_tar_entry(int src_fh, unsigned long *total_count, char *md5sum);
int cmp_tar_entries(const void *te1, const void *te2);
int command_pipes(const char *command, const char *args, int *pipes);

int main(int argc, char **argv)
{
	int src_fh;
	int trg_fh;
        struct tar_entry *source, *target, entry;
        char source_md5[32], target_md5[32];
        unsigned long source_count, target_count;
	unsigned long x;
	char src_common[256], trg_common[256];  /* common dir's... */
	unsigned int src_common_len=0, trg_common_len=0;
	char *text = "debianutils-1.16.7/which.1";
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
        /* this next one is moreso for bsearch's, but it's prob useful for the common-prefix alg too */
        qsort((struct tar_entry *)target, target_count, sizeof(struct tar_entry), cmp_tar_entries);
        
        
        /* alg to basically figure out the common dir prefix... eg, if everything is in dir debianutils-1.16.3
           one thing I do wonder about... I make user of ptr algebra, do these methods hold for all architectures?
           ergo, do all arch's storage of a string have for ptr's string[x] < then string[x+1]? */
        /*note, we want the slash, hence +1 */
        src_common_len=(char *)rindex((const char *)source->fullname, '/') - (char *)source->fullname+1;
        strncpy((char *)src_common, (char *)source->fullname,src_common_len);
        src_common[src_common_len] = '\0';  /*null delimit it */
        for (x=0; x < source_count; x++) {
            if (strncmp((const char *)src_common, (const char *)source[x].fullname, src_common_len) !=0) {
                char *p;
                /* null the / at src_common_len-1, and attempt rindex again. */
                src_common[src_common_len -1]='\0';
                if((p = rindex(src_common, '/'))==NULL){
                    src_common_len=0;
                    src_common[0]='\0'; /*no common dir prefix. damn. */
                } else {
                    //printf("p=%u\n",p);
                    src_common_len= src_common - p + 1; /*include the / again... */
                    src_common[src_common_len +1]='\0';
                }
            }
        }
        printf("new src_common='%.255s'\n", src_common);
        trg_common_len=(char *)rindex((const char *)target->fullname, '/') - (char *)target->fullname+1;
        strncpy((char *)trg_common, (char *)target->fullname,trg_common_len);
        trg_common[trg_common_len] = '\0';  /* null delimit it */
        for (x=0; x < target_count; x++) {
            if (strncmp((const char *)trg_common, (const char *)target[x].fullname, trg_common_len) !=0) {
                char *p;
                /* null the / at trg_common_len-1, and attempt rindex again. */
                trg_common[trg_common_len -1]='\0';
                if((p = rindex(trg_common, '/'))==NULL){
                    trg_common_len=0;
                    trg_common[0]='\0'; /*no common dir prefix. damn. */
                } else {
                    //printf("p=%u\n",p);
                    trg_common_len= trg_common - p + 1; /*include the / again... */
                    trg_common[trg_common_len +1]='\0';
                }
            }
        }
        printf("new trg_common='%.255s'\n", trg_common);
        /*perhaps this is a crappy method, but basically for the my sanity, just up the fullname ptr
         to remove the common-prefix.  wonder if string functions behave and don't go past the sp... */
        /* init the fullname_ptr to point to the char after the common-prefix dir.  if no prefix, points
        to the start of fullname. */
        /*note for harring.  deref fullname, add common_len, then assign to ptr after casting */
        for (x=0; x < source_count; x++)
            source[x].fullname_ptr= (char *)&source[x].fullname + src_common_len;
        for (x=0; x < target_count; x++) 
            target[x].fullname_ptr = (char *)&target[x].fullname + trg_common_len;
        /*printf("attempting bsearch lookup...\n");*/
        /* strncpy((char *)entry.name, text,strlen(text));*/
        /*printf("verifying something, name=%s\n", (char *)entry.name);*/
        /* note the funky nature, cast the returned void ptr to tar_entry, then dereference it */
        /*entry = (struct tar_entry)*((struct tar_entry *)bsearch((const void *)&entry, (const void *)target,
            target_count, sizeof(struct tar_entry), cmp_tar_entries));
        printf("heh, trying something\n");*/
        /*if (&entry != NULL)
            printf("name='%.100s'\n", entry.name);
        else
            printf("well, key(%s) was not found\n", text);*/
        free(source);
        free(target);
	close(src_fh);
	close(trg_fh);

}

int cmp_tar_entries(const void *te1, const void *te2)
{
    /* basically check to see if prefix has anything.  if not, do a strncmp based on just name */
    /*if (strnlen((char *)((struct tar_entry *)te1)->prefix, 155)!=0 && 
        strnlen((char *)((struct tar_entry *)te2)->prefix, 155)!=0) {
        return(strncmp((char *)((struct tar_entry *)te1)->prefix, (char *)((struct tar_entry *)te2)->prefix,155) ||
           strncmp((char *)((struct tar_entry *)te1)->name, (char *)((struct tar_entry *)te2)->name,100));
    } else {
        return(strncmp((char *)((struct tar_entry *)te1)->name, (char *)((struct tar_entry *)te2)->name,100));
    }*/
    return(strncmp((char *)((struct tar_entry *)te1)->fullname, (char *)((struct tar_entry *)te2)->fullname, 255));
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

struct tar_entry *read_fh_to_tar_entry(int src_fh, unsigned long *total_count, char *md5sum)
{
    struct tar_entry *file, *entry;
    //struct tar_llist source, target, *src_ptr, *trg_ptr;
    char *entry_char[512];
    //unsigned int count=0, offset=0;
    unsigned long offset=0, array_size=50000;
    unsigned long count =0;
    unsigned int read_bytes;
    int pipes[2];
    if((file = (struct tar_entry *)calloc(array_size,sizeof(struct tar_entry)))==NULL){
	    perror("crud, couldn't allocate necesary memory.  What gives?\n");
	    exit(EXIT_FAILURE);
	}
    /*printf("setting array_size==%u\n",array_size);
    printf("opening md5sum pipes\n");*/
    if(command_pipes("md5sum", "-", pipes)){
        perror("failed opening md5sum pipes, wtf?\n");
        exit(EXIT_FAILURE);
    }
    //printf("file=%u, tp=%u\n", file, tp);
    while((read_bytes=read(src_fh, entry_char, 512))==512 && strnlen(entry_char)!=0) {
        write(pipes[1], entry_char, 512);
        entry = convert_str_tar_entry((char *)entry_char);
        entry->entry_num = count;
        entry->file_loc = offset;
        if (entry->size !=0) {
            int x= entry->size>>9;
            if (entry->size % 512)
                x++;
            //lseek(src_fh, (long)(x * 512), 1);
            offset += x + 1;
            while(x-- > 0){
                if(read(src_fh, entry_char, 512)==512){
                    write(pipes[1], entry_char, 512);
                } else 
                    perror("Unexpected end of file encountered, exiting\n");
            }                
        } else {
            offset++;
        }
        if(count==array_size) {
            /* out of room, resize */
            if ((file=(struct tar_entry *)realloc(file,(array_size+=50000)*sizeof(struct tar_entry)))==NULL){
                perror("Eh?  Ran out of room for file array...\n");
                exit(EXIT_FAILURE);
            /*
            } else {
                printf("resized array to %u\n", array_size);
            */
            }
            //array_size *= 5;
        }
        file[count++] = *entry;
        /*printf("0 :%.100s\n1 :%u\n2 :%u\n3 :%u\n4 :%u\n5 :%.12s\n6 :%u\n7 :%c\n8 :%.100s\n9 :%.6s\n10:%.2s\n11:%.32s\n12:%.32s\n13:%u\n14:%u\n15:%100s\n16:%u\n",
        entry.name, entry.mode, entry.uid, entry.gid, entry.size, entry.mtime, entry.chksum, entry.typeflag,
        entry.linkname, entry.magic, entry.version, entry.uname, entry.gname, entry.devmajor, entry.devminor,
        entry.prefix, entry.file_loc);*/
    }
    *total_count = count;
    if(read_bytes>0) { /*finish outputing the file for md5summing */
        write(pipes[1], entry_char, read_bytes);
        while((read_bytes = read(src_fh, entry_char, 512))>0)
            write(pipes[1], entry_char, read_bytes);
    }
    close(pipes[1]);  /* close the write pipe to md5 */
    if ((count=read(pipes[0], entry_char, 32))!=32) {
        perror("thats weird, md5sum didn't return 32 char's... bug most likely in diffball.\n");
        exit(EXIT_FAILURE);
    }
    memcpy((char *)md5sum, (char *)entry_char, 32);
    //printf("md5sum='%.32s'\n", entry_char);
    //while(write(pipes[1], entry_char, read(src_fh, entry_char, 512)));
    //printf("source has %lu files, reallocing from %u to %u\n", count, array_size, count);
    /* free up the unused memory of the array */
    if ((file=(struct tar_entry *)realloc(file,count*sizeof(struct tar_entry)))==NULL){
        perror("Shit.\nNo explanation, just Shit w/ a capital S.\n");
        exit(EXIT_FAILURE);
    }
    return file;
}
