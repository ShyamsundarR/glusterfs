xlator_LTLIBRARIES = posix2-mds.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/storage

posix2_mds_la_LDFLAGS = -module -avoid-version

posix2_mds_la_SOURCES = posix2-mds.c zfstore.c zfstore-dir.c zfstore-file.c zfstore-handle.c \
                        zfxattr.c

posix2_mds_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
                       $(top_builddir)/xlators/storage/posix2/common/src/libposix2.la \
                       $(LIBAIO) $(ACL_LIBS)

noinst_HEADERS = posix2-mds.h posix2-messages.h super.h zfstore.h zfxattr.h zfstore-handle.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src \
            -I$(top_srcdir)/rpc/xdr/src \
	    -I$(top_srcdir)/rpc/rpc-lib/src \
	    -I$(top_srcdir)/xlators/storage/posix2/common/src \
            -I$(CONTRIBDIR)/crc32c

AM_CFLAGS = -fno-strict-aliasing -Wall $(GF_CFLAGS)

CLEANFILES =

