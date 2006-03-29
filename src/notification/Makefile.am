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

# mini popup plugin
liblinotifpopup_la_LIBADD = $(PACKAGE_LIBS)
liblinotifpopup_la_CFLAGS = $(PACKAGE_FLAGS)
liblinotifpopup_la_SOURCES = notif_popup.c notif_popup.h


pkglib_LTLIBRARIES = liblinotifpopup.la
