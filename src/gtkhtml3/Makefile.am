## Process this file with automake to produce Makefile.in

pkglib_LTLIBRARIES = liblihtmlg.la

liblihtmlg_la_SOURCES = gtkhtml3.c
liblihtmlg_la_LIBADD = $(PACKAGE_LIBS) $(GTKHTML3_LIBS) $(INTLLIBS)
liblihtmlg_la_CFLAGS = $(PACKAGE_CFLAGS) $(GTKHTML3_CFLAGS)
liblihtmlg_la_LDFLAGS = -avoid-version -module
