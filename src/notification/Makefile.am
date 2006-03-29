## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)

# plugin loading code
noinst_LIBRARIES = liblinotification.a

liblinotification_a_SOURCES = notif_plugin.c notif_plugin.h 

liblinotification_a_CFLAGS = $(PACKAGE_FLAGS)


