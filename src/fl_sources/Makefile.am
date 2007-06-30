## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)
	
noinst_LIBRARIES = libliflsources.a
libliflsources_a_SOURCES = node_source.c node_source.h \
                           default_source.c default_source.h \
                           dummy_source.c dummy_source.h \
                           google_source.c google_source.h \
                           opml_source.c opml_source.h \
			   bloglines_source.c bloglines_source.h

libliflsources_a_CFLAGS = $(PACKAGE_FLAGS)

gladedir = $(pkgdatadir)
glade_DATA = google_source.glade node_source.glade opml_source.glade bloglines_source.glade

EXTRA_DIST = $(glade_DATA)
