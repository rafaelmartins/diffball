
AM_CFLAGS = -ftest-coverage -fprofile-arcs -pg -ftest-coverage -Wall 
#AM_CFLAGS = -Wall 

FORMATS = dcbuffer.c cfile.c bit-functions.c gdiff.c switching.c bdiff.c \
    bdelta.c udiff.c line-util.c
DIFF_FILES = diff-algs.c adler32.c primes.c
CFILE_LIBS = -lssl -lcrypto

bin_PROGRAMS = differ patcher diffball convert_delta ctest

ctest_SOURCES = ctest.c cfile.c
ctest_LDADD = $(CFILE_LIBS)
diffball_LDADD = $(CFILE_LIBS)
differ_LDADD   = $(CFILE_LIBS)
convert_delta_LDADD = $(CFILE_LIBS)
patcher_LDADD = $(CFILE_LIBS)
patcher_SOURCES = patcher.c apply-patch.c $(FORMATS)
diffball_SOURCES = diffball.c string-misc.c tar.c $(DIFF_FILES) $(FORMATS) 
differ_SOURCES = differ.c $(DIFF_FILES) $(FORMATS)
convert_delta_SOURCES = convert_delta.c  $(FORMATS)

include_HEADERS = cfile.h dcbuffer.h diff-algs.h bit-functions.h gdiff.h \
	apply-patch.h adler32.h switching.h primes.h string-misc.h \
	data-structs.h tar.h bdiff.h xdelta1.h cfile.h bdelta.h line-util.h \
	udiff.h defs.h

