## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)

# generic notification support
noinst_LIBRARIES = libnotification.a

libnotification_a_SOURCES = notification.c notification.h 
libnotification_a_CFLAGS = $(PACKAGE_CFLAGS)

if WITH_LIBNOTIFY

# libnotify popup plugin
liblinotiflibnotify_la_LIBADD = $(PACKAGE_LIBS) $(LIBNOTIFY_LIBS)
liblinotiflibnotify_la_CFLAGS = $(PACKAGE_CFLAGS) $(LIBNOTIFY_CFLAGS)
liblinotiflibnotify_la_SOURCES = libnotify.c notification.h
liblinotiflibnotify_la_LDFLAGS = -avoid-version -module

pkglib_LTLIBRARIES = liblinotiflibnotify.la
endif
