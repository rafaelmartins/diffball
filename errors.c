#include <stdio.h>
#include <errno.h>
#include "defs.h"
#include "cfile.h"

void
print_exit(int err)
{
    switch(err) {
	case PATCH_TRUNCATED:
	printf("truncated patch detected, exiting\n");
	break;
	case PATCH_CORRUPT_ERROR:
	printf("patch corruptioned detected, exiting\n");
	break;
	case IO_ERROR:
	printf("io/bus error, exiting\n");
	break;
	case EOF_ERROR:
	printf("unexpected eof detected, exiting\n");
	break;
	case MEM_ERROR:
	printf("not enough memory, exiting\n");
	break;
	case FORMAT_ERROR:
	printf("This format is too limited to encode this delta, exiting\n");
	break;
	case DATA_ERROR:
	printf("data error detected, exiting\n");
	break;
    }
    exit(EXIT_FAILURE);
}
