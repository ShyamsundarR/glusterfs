noinst_LTLIBRARIES = libposix2.la

libposix2_la_CFLAGS = $(GF_CFLAGS) $(GF_DARWIN_LIBGLUSTERFS_CFLAGS) \
                      -DDATADIR=\"$(localstatedir)\"

libposix2_la_CPPFLAGS = $(GF_CPPFLAGS) -D__USE_FILE_OFFSET64

libposix2_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

libposix2_la_SOURCES = posix2-common.c

noinst_HEADERS = posix2.h posix2-mem-types.h
