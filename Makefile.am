AUTOMAKE_OPTIONS        = dist-bzip2 no-dist-gzip

if COMPILE_PROFILING_CFLAGS
    EXTRA_CFLAGS = -ftest-coverage -fprofile-arcs -pg -ftest-coverage 
else
    EXTRA_CFLAGS =
endif
AM_CFLAGS = -Wall -D__USE_GNU $(EXTRA_CFLAGS)

FORMATS = cfile.c bit-functions.c gdiff.c switching.c bdiff.c \
    bdelta.c xdelta1.c udiff.c bsdiff.c fdtu.c line-util.c formats.c
DCB_FILES  = dcbuffer.c dcb-cfh-funcs.c command_list.c
DIFF_FILES = diff-algs.c adler32.c primes.c hash.c $(DCB_FILES)

#ctest_SOURCES = ctest.c cfile.c
#hash_stats_SOURCES = hash_stats.c $(DIFF_FILES) options.c cfile.c

if BUILD_DEBUG_CFILE
    cfile_bins = ctest
else
    cfile_bins = 
endif
if BUILD_DEBUG_HASH
    hash_bins = hash_stats
else
    hash_bins =
endif

if COMPILE_PROFILING_CFLAGS
    lib_LTLIBRARIES = libdiffball.la
else
    lib_LTLIBRARIES =
endif

libdiffball_la_SOURCES = $(FORMATS) $(DIFF_FILES)

if COMPILE_PROFILING_CFLAGS
   DIFF_EXTRAS = ${DIFF_FILES} ${FORMATS}
   DIFF_LIBS =
else
   DIFF_EXTRAS =
   DIFF_LIBS = libdiffball.la
endif

bin_PROGRAMS = differ patcher diffball convert_delta ${cfile_bins} \
    ${hash_bins}

patcher_LDADD	=	${DIFF_LIBS} 
patcher_SOURCES = 	patcher.c	\
			apply-patch.c	\
			options.c	\
			errors.c

diffball_LDADD	= 	${DIFF_LIBS} 
diffball_SOURCES = 	diffball.c	\
			string-misc.c	\
			tar.c 		\
			options.c 	\
			errors.c ${DIFF_EXTRAS}

differ_LDADD	=	${DIFF_LIBS}
differ_SOURCES = 	differ.c 	\
			options.c 	\
			errors.c  ${DIFF_EXTRAS}

convert_delta_LDADD   =	${DIFF_LIBS}
convert_delta_SOURCES = convert_delta.c	\
			options.c 	\
			errors.c ${DIFF_EXTRAS}
man_MANS = differ.1 diffball.1 patcher.1 convert_delta.1
EXTRA_DIST = $(man_MANS)

noinst_HEADERS = cfile.h dcbuffer.h diff-algs.h bit-functions.h gdiff.h \
	apply-patch.h adler32.h switching.h primes.h string-misc.h \
	tar.h bdiff.h xdelta1.h cfile.h bdelta.h line-util.h \
	udiff.h fdtu.h bsdiff.h defs.h options.h formats.h hash.h errors.h \
	dcb-cfh-funcs.h command_list.h


