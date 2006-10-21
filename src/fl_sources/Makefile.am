## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)
	
# plugin loading code
noinst_LIBRARIES = libliflsources.a

libliflsources_a_SOURCES = node_source.c node_source.h \
                           node_source-ui.c node_source-ui.h \
                           default_source.c default_source.h \
                           dummy_source.c dummy_source.h \
                           opml_source.c opml_source.h \
                           opml_source-ui.c opml_source-ui.h \
                           opml_source-cb.c opml_source-cb.h \
			   bloglines_source.c bloglines_source.h \
			   bloglines_source-ui.c bloglines_source-ui.h \
			   bloglines_source-cb.c bloglines_source-cb.h

libliflsources_a_CFLAGS = $(PACKAGE_FLAGS)

