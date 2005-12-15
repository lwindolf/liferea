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

pkglib_LTLIBRARIES = liblifldefault.la \
                     libliflopml.la

# default feed list plugin
liblifldefault_la_LIBADD = $(PACKAGE_LIBS)
liblifldefault_la_CFLAGS = $(PACKAGE_FLAGS)
liblifldefault_la_SOURCES = fl_common.c fl_common.h \
                            fl_default.c fl_default.h

# OPML/Planet plugin target
libliflopml_la_LIBADD = $(PACKAGE_LIBS)
libliflopml_la_CFLAGS = $(PACKAGE_FLAGS)
libliflopml_la_SOURCES = fl_common.c fl_common.h \
                         fl_opml.c fl_opml.h \
                         fl_opml-ui.c fl_opml-ui.h \
                         fl_opml-cb.c fl_opml-cb.h


# FIXME: library targets for:
#	fl_bloglines.c fl_bloglines.h

