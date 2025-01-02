## Process this file with automake to produce Makefile.in

SUBDIRS = web_extension
AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
        -I$(top_srcdir)/src \
        -DWEB_EXTENSIONS_DIR=\""$(pkglibdir)/web-extension"\"

noinst_LIBRARIES = libwebkit.a

libwebkit_a_CFLAGS = $(PACKAGE_CFLAGS)
libwebkit_a_SOURCES = liferea_webkit.c liferea_webkit.h liferea_web_view.c liferea_web_view.h
