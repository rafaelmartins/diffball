lib_LTLIBRARIES = libcfile.la
AM_CFLAGS = -I$(top_builddir)/include/
libcfile_la_SOURCES = cfile.c
libcfile_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(LIBLZMA_CFLAGS)
libcfile_la_LIBADD = \
	$(AM_LIBADD) \
	$(LIBLZMA_LIBS)
noinst_HEADERS = defs.h
