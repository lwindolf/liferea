## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ $(MOZILLA_CFLAGS)


MOZILLA = liblihtmlm.la
MOZILLA_CFLAGS = @MOZILLA_CFLAGS@ -Wall

lib_LTLIBRARIES = $(MOZILLA)

liblihtmlm_la_SOURCES = \
	mozembed.c \
	mozilla.cpp mozilla.h
	
liblihtmlm_la_LIBADD = @PACKAGE_LIBS@ @MOZILLA_LIBS@ $(INTLLIBS)
