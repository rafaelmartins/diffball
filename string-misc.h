#ifndef _HEADER_STRING_MISC
#define _HEADER_STRING_MISC 1
#include <string.h>
#include <stdio.h>
#include "config.h"

#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t maxlen);
#endif

#endif

