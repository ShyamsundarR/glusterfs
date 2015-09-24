xlator_LTLIBRARIES = posix2-mds.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/storage

posix2_mds_la_LDFLAGS = -module -avoid-version

posix_mds_la_SOURCES = posix2-mds.c xattrstore.c xattrstore-dir.c \
                       $(top_srcdir)/xlators/storage/posix2/common/src/posix2-common.c

posix_mds_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la $(LIBAIO) \
                      $(ACL_LIBS)

noinst_HEADERS = posix2-mds.h posix2-messages.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src \
            -I$(top_srcdir)/rpc/xdr/src \
	    -I$(top_srcdir)/rpc/rpc-lib/src \
	    -I$(top_srcdir)/xlators/storage/posix2/common

AM_CFLAGS = -fno-strict-aliasing -Wall $(GF_CFLAGS)

CLEANFILES =

