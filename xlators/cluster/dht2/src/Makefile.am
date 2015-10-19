xlator_LTLIBRARIES = dht2.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/cluster

dht2_sources = dht2-main.c dht2-helpers.c dht2-layout.c

dht2_ext_sources = $(top_builddir)/xlators/lib/src/libxlator.c

dht2_ext_headers = $(top_builddir)/xlators/lib/src/libxlator.h

dht2_la_LDFLAGS = -module -avoid-version
dht2_la_SOURCES = $(dht2_sources) $(dht2_ext_sources) $(dht2_ext_headers)
dht2_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

AM_CFLAGS = -Wall $(GF_CFLAGS)

AM_CPPFLAGS = $(GF_CPPFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/libglusterfs/src
AM_CPPFLAGS += -I$(top_srcdir)/xlators/lib/src

CLEANFILES =

uninstall-local:
	rm -f $(DESTDIR)$(xlatordir)/distribute2.so

install-data-hook:
	ln -sf dht2.so $(DESTDIR)$(xlatordir)/distribute2.so
