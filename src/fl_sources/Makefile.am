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
			   .node_source-cb.c .node_source-cb.h \
                           default_source.c default_source.h \
                           dummy_source.c \
                           opml_source.c opml_source.h \
                           opml_source-ui.c opml_source-ui.h \
                           opml_source-cb.c opml_source-cb.h 

libliflsources_a_CFLAGS = $(PACKAGE_FLAGS)

