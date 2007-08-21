## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src

# plugin loading code
noinst_LIBRARIES = liblisync.a

liblisync_a_SOURCES =	avahi_publisher.c avahi_publisher.h \
			avahi_browser.c avahi_browser.h
			
liblisync_a_CFLAGS = $(PACKAGE_CFLAGS) $(AVAHI_CFLAGS)


