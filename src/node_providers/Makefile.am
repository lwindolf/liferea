## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS)
	
noinst_LIBRARIES = liblinode_providers.a
liblinode_providers_a_SOURCES = \
        feed.c feed.h \
        folder.c folder.h \
        newsbin.c newsbin.h \
        vfolder.c vfolder.h

liblinode_providers_a_CFLAGS = $(PACKAGE_FLAGS)
