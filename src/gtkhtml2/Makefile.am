## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ $(GTKHTML2_CFLAGS)


GTKHTML2 = liblihtmlg.la
GTKHTML2_CFLAGS = @GTKHTML2_CFLAGS@ -Wall

lib_LTLIBRARIES = $(GTKHTML2)

liblihtmlg_la_SOURCES = gtkhtml2.c
liblihtmlg_la_LIBADD = @PACKAGE_LIBS@ @GTKHTML2_LIBS@ $(INTLLIBS)
liblihtmlg_la_LDFLAGS = -avoid-version -module