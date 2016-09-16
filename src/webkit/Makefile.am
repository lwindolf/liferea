## Process this file with automake to produce Makefile.in

SUBDIRS = web_extension
AM_CPPFLAGS = -I$(top_srcdir)/src  $(PACKAGE_CFLAGS) $(WEBKIT_CFLAGS) -DWEB_EXTENSIONS_DIR=\""$(pkglibdir)/web-extension"\"

noinst_LIBRARIES = libwebkit.a

libwebkit_a_SOURCES = webkit.c liferea_web_view.c
#libwebkit_a_LIBADD = $(PACKAGE_LIBS) $(WEBKIT_LIBS) $(INTLLIBS)
libwebkit_a_CFLAGS = $(PACKAGE_CFLAGS) $(WEBKIT_CFLAGS)

