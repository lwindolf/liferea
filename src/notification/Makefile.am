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

liblinotification_a_CFLAGS = $(PACKAGE_CFLAGS)

if WITH_LIBNOTIFY

# libnotify popup plugin
liblinotiflibnotify_la_LIBADD = $(PACKAGE_LIBS) $(LIBNOTIFY_LIBS)
liblinotiflibnotify_la_CFLAGS = $(PACKAGE_CFLAGS) $(LIBNOTIFY_CFLAGS)
liblinotiflibnotify_la_SOURCES = notif_libnotify.c notif_libnotify.h notif_plugin.h 

pkglib_LTLIBRARIES = liblinotiflibnotify.la
endif
