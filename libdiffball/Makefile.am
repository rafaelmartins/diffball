#udiff.c is intentionally left out, as is line-util.c

FORMATS = bit-functions.c gdiff.c switching.c bdiff.c \
    bdelta.c xdelta1.c bsdiff.c fdtu.c formats.c

DCB_FILES  = dcbuffer.c dcb-cfh-funcs.c command_list.c
DIFF_FILES = diff-algs.c adler32.c primes.c hash.c $(DCB_FILES) errors.c apply-patch.c

libdiffball_la_CFLAGS = $(AM_CFLAGS) -I$(top_builddir)/include
lib_LTLIBRARIES = libdiffball.la
libdiffball_la_LIBADD = $(top_builddir)/libcfile/libcfile.la
libdiffball_la_SOURCES = $(FORMATS) $(DIFF_FILES)



