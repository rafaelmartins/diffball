
#AM_CFLAGS = -ftest-coverage -fprofile-arcs -pg -ftest-coverage -Wall 
AM_CFLAGS = -Wall 
bin_PROGRAMS = differ patcher diffball convert_delta ctest
diffball_SOURCES = diffball.c diff-algs.c string-misc.c tar.c primes.c \
	bit-functions.c cfile.c adler32.c dcbuffer.c gdiff.c switching.c \
	bdiff.c bdelta.c

ctest_SOURCES = ctest.c cfile.c
ctest_LDADD = -lssl -lcrypto
diffball_LDADD = -lssl -lcrypto
differ_LDADD   = -lssl -lcrypto
convert_delta_LDADD = -lssl -lcrypto
patcher_LDADD = -lssl -lcrypto
patcher_SOURCES = patcher.c cfile.c dcbuffer.c bit-functions.c apply-patch.c \
	gdiff.c bdiff.c switching.c xdelta1.c bdelta.c

differ_SOURCES = differ.c diff-algs.c dcbuffer.c dcbuffer.h cfile.c \
	bit-functions.c primes.c adler32.c gdiff.c bdiff.c \
	switching.c xdelta1.c bdelta.c
#switching.c bdiff.c
convert_delta_SOURCES = convert_delta.c cfile.c dcbuffer.c gdiff.c \
	switching.c bdelta.c bdiff.c bit-functions.c

include_HEADERS = cfile.h dcbuffer.h diff-algs.h bit-functions.h gdiff.h \
	apply-patch.h adler32.h switching.h primes.h string-misc.h \
	data-structs.h tar.h bdiff.h xdelta1.h cfile.h bdelta.h
