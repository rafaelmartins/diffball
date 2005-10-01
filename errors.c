#include <stdio.h>
#include "defs.h"
#include "cfile.h"
#include "errors.h"

void
print_error(int err)
{
	switch(err) {
		case PATCH_TRUNCATED:
		fprintf(stderr,"truncated patch detected, exiting\n");
		break;
		case PATCH_CORRUPT_ERROR:
		fprintf(stderr,"patch corruption detected, exiting\n");
		break;
		case IO_ERROR:
		fprintf(stderr,"io/bus error, exiting\n");
		break;
		case EOF_ERROR:
		fprintf(stderr,"unexpected eof detected, exiting\n");
		break;
		case MEM_ERROR:
		fprintf(stderr,"not enough memory, exiting\n");
		break;
		case FORMAT_ERROR:
		fprintf(stderr,"This format is too limited to encode this delta, exiting\n");
		break;
		case DATA_ERROR:
		fprintf(stderr,"data error detected, exiting\n");
		break;
		default:
		fprintf(stderr,"wtf, error %i\n", err);
	}
}
