
bin_PROGRAMS = differ patcher diffball
diffball_SOURCES = diffball.c diff-algs.c gdiff.c string-misc.c tar.c \
		  primes.c bit-functions.c cfile.c adler32.c dcbuffer.c

diffball_LDADD = -lssl -lcrypto
patcher_SOURCES = patcher.c cfile.c dcbuffer.c gdiff.c bit-functions.c \
	apply-patch.c switching.c
differ_SOURCES = differ.c diff-algs.c dcbuffer.c dcbuffer.h cfile.c \
	bit-functions.c gdiff.c switching.c adler32.c primes.c
include_HEADERS = cfile.h dcbuffer.h diff-algs.h bit-functions.h gdiff.h \
	apply-patch.h adler32.h switching.h primes.h string-misc.h \
	data-structs.h tar.h
