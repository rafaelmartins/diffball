AUTOMAKE_OPTIONS	= dist-bzip2 no-dist-gzip

AM_CFLAGS = -Wall -D_GNU_SOURCE -I$(top_builddir)/include/
SUBDIRS = libcfile libdiffball include man

if BUILD_DEBUG_CFILE
	cfile_bins = ctest
else
	cfile_bins =
endif

DIFF_LIBS = libdiffball/libdiffball.la
CFILE_LIBS = libcfile/libcfile.la
bin_PROGRAMS = differ patcher diffball convert_delta ${cfile_bins}

patcher_LDADD	=	${DIFF_LIBS} ${CFILE_LIBS}
patcher_SOURCES = 	patcher.c	\
					options.c

diffball_LDADD	= 	${DIFF_LIBS} ${CFILE_LIBS}
diffball_SOURCES = 	diffball.c	\
					string-misc.c	\
					tar.c 		\
					options.c

differ_LDADD	=	${DIFF_LIBS} ${CFILE_LIBS}
differ_SOURCES = 	differ.c 	\
					options.c 

convert_delta_LDADD   =	${DIFF_LIBS} ${CFILE_LIBS}
convert_delta_SOURCES = convert_delta.c	\
						options.c

#man_MANS = differ.1 diffball.1 patcher.1 convert_delta.1
#EXTRA_DIST = $(man_MANS)

# line-util.h is left out, as is udiff.h
noinst_HEADERS = string-misc.h tar.h options.h

dist-hook:
	git log --format=short > $(distdir)/ChangeLog
