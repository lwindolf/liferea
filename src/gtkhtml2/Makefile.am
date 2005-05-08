## Process this file with automake to produce Makefile.in

INCLUDES = @PACKAGE_CFLAGS@ @GTKHTML2_CFLAGS@ $(DBUS_CFLAGS)

pkglib_LTLIBRARIES = liblihtmlg.la

liblihtmlg_la_SOURCES = gtkhtml2.c
liblihtmlg_la_LIBADD = $(PACKAGE_LIBS) $(GTKHTML2_LIBS) $(INTLLIBS)
liblihtmlg_la_CFLAGS = $(PACKAGE_CFLAGS) $(GTKHTML2_CFLAGS)
liblihtmlg_la_LDFLAGS = -avoid-version -module
