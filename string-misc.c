#include "string-misc.h"

#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t maxlen)
{
	size_t count=0;
	while(count < maxlen && '\0' != s[count])
		count++;
	return (size_t)count;
}
#endif

