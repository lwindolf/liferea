## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ $(GTKHTML3_CFLAGS)

GTKHTML3 = liblihtmlh.la
GTKHTML3_CFLAGS = @GTKHTML3_CFLAGS@

lib_LTLIBRARIES = $(GTKHTML3)

liblihtmlh_la_SOURCES = gtkhtml3.c
liblihtmlh_la_LIBADD = @PACKAGE_LIBS@ @GTKHTML3_LIBS@ $(INTLLIBS)
