## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)

noinst_LIBRARIES = libliflprovider.a

libliflprovider_a_CFLAGS = $(PACKAGE_FLAGS)
libliflprovider_a_SOURCES = \
	fl_plugin.h \
	fl_default.c fl_default.h \
	fl_opml.c fl_opml.h \
	fl_bloglines.c fl_bloglines.h
