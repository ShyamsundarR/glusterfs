
xlator_LTLIBRARIES = posix2.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/storage

posix2_la_LDFLAGS = -module -avoid-version

posix2_la_SOURCES = posix2.c posix2-helpers.c
posix2_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la $(LIBAIO) \
                  $(ACL_LIBS)

noinst_HEADERS = posix2.h posix2-mem-types.h posix2-handle.h posix2-messages.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src \
            -I$(top_srcdir)/rpc/xdr/src \
            -I$(top_srcdir)/rpc/rpc-lib/src

AM_CFLAGS = -fno-strict-aliasing -Wall $(GF_CFLAGS)

CLEANFILES =

