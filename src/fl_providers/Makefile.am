## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)

# plugin loading code
noinst_LIBRARIES = libliflprovider.a

libliflprovider_a_SOURCES = fl_plugin.c fl_plugin.h
libliflprovider_a_CFLAGS = $(PACKAGE_FLAGS)

# default feed list plugin
pkglib_LTLIBRARIES = liblifldefault.la

liblifldefault_la_LIBADD = $(PACKAGE_LIBS)
liblifldefault_la_CFLAGS = $(PACKAGE_FLAGS)
liblifldefault_la_SOURCES = fl_default.c fl_default.h

# FIXME: library targets for:
#	fl_opml.c fl_opml.h 
#	fl_bloglines.c fl_bloglines.h

