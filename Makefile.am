#AM_CFLAGS = -ftest-coverage -fprofile-arcs -pg -ftest-coverage -Wall 
AM_CFLAGS = -Wall

FORMATS = cfile.c bit-functions.c gdiff.c switching.c bdiff.c \
    bdelta.c xdelta1.c udiff.c bsdiff.c line-util.c formats.c
DIFF_FILES = diff-algs.c adler32.c primes.c dcbuffer.c hash.c

bin_PROGRAMS = differ patcher diffball convert_delta #hash_stats ctest

#ctest_SOURCES = ctest.c cfile.c
#hash_stats_SOURCES = hash_stats.c $(DIFF_FILES) options.c cfile.c

patcher_SOURCES = patcher.c apply-patch.c options.c $(FORMATS) dcbuffer.c
diffball_SOURCES = diffball.c string-misc.c tar.c options.c \
    $(DIFF_FILES) $(FORMATS) 
differ_SOURCES = differ.c $(DIFF_FILES) options.c $(FORMATS)
differ_LDADD = -Lm
convert_delta_SOURCES = convert_delta.c options.c $(FORMATS) dcbuffer.c
man_MANS = differ.1 diffball.1 patcher.1 convert_delta.1
EXTRA_DIST = $(man_MANS)

noinst_HEADERS = cfile.h dcbuffer.h diff-algs.h bit-functions.h gdiff.h \
	apply-patch.h adler32.h switching.h primes.h string-misc.h \
	data-structs.h tar.h bdiff.h xdelta1.h cfile.h bdelta.h line-util.h \
	udiff.h defs.h options.h formats.h hash.h

