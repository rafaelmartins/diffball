#ifndef _HEADER_APPLY_PATCH
#define _HEADER_APPLY_PATCH 1

void reconstructFile(struct CommandBuffer *dcbuff, struct cfile * src_cfh, 
	struct cfile *delta_cfh, struct cfile *out_cfh);

#endif _HEADER_APPLY_PATCH