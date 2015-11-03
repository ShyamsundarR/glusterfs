xlator_LTLIBRARIES = dht2.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/cluster

dht2_sources = dht2-main.c dht2-helpers.c dht2-layout.c

dht2_ext_sources = $(top_builddir)/xlators/lib/src/libxlator.c

dht2_ext_headers = $(top_builddir)/xlators/lib/src/libxlator.h

dht2_la_LDFLAGS = -module -avoid-version
dht2_la_SOURCES = $(dht2_sources) $(dht2_ext_sources) $(dht2_ext_headers)
dht2_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

noinst_PYTHON = dht2-autogen-fops-c.py dht2-autogen-fops-h.py

nodist_dht2_la_SOURCES = dht2-autogen-fops.c dht2-autogen-fops.h

BUILT_SOURCES = dht2-autogen-fops.c dht2-autogen-fops.h

EXTRA_DIST = dht2-autogen-fops-tmpl.c dht2-autogen-fops-tmpl.h

AM_CFLAGS = -Wall $(GF_CFLAGS)

AM_CPPFLAGS = $(GF_CPPFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/libglusterfs/src
AM_CPPFLAGS += -I$(top_srcdir)/xlators/lib/src

uninstall-local:
	rm -f $(DESTDIR)$(xlatordir)/distribute2.so

install-data-hook:
	ln -sf dht2.so $(DESTDIR)$(xlatordir)/distribute2.so

dht2-autogen-fops.c: dht2-autogen-fops-tmpl.c dht2-autogen-fops-c.py
	PYTHONPATH=$(PYTHONPATH):../../../../libglusterfs/src/ $(PYTHON) \
		$(srcdir)/dht2-autogen-fops-c.py \
		$(srcdir)/dht2-autogen-fops-tmpl.c > $@

dht2-autogen-fops.h: dht2-autogen-fops-tmpl.h dht2-autogen-fops-h.py
	PYTHONPATH=$(PYTHONPATH):../../../../libglusterfs/src/ \
		$(PYTHON) $(srcdir)/dht2-autogen-fops-h.py \
		$(srcdir)/dht2-autogen-fops-tmpl.h > $@

CLEANFILES = dht2-autogen-fops.c dht2-autogen-fops.h
