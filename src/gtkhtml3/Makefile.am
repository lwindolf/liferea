## Process this file with automake to produce Makefile.in

pkglib_LTLIBRARIES = liblihtmlh.la

liblihtmlh_la_SOURCES = gtkhtml3.c
liblihtmlh_la_LIBADD = $(PACKAGE_LIBS) $(GTKHTML3_LIBS) $(INTLLIBS)
liblihtmlh_la_CFLAGS = $(PACKAGE_CFLAGS) $(GTKHTML3_CFLAGS)
liblihtmlh_la_LDFLAGS = -avoid-version -module
