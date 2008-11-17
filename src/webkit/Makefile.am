## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src  $(PACKAGE_CFLAGS) $(WEBKIT_CFLAGS)

noinst_LIBRARIES = libwebkit.a

libwebkit_a_SOURCES = webkit.c
#libwebkit_a_LIBADD = $(PACKAGE_LIBS) $(WEBKIT_LIBS) $(INTLLIBS)
libwebkit_a_CFLAGS = $(PACKAGE_CFLAGS) $(WEBKIT_CFLAGS)

