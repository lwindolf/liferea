## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)

if WITH_LIBNOTIFY

libnotify_a_CFLAGS = $(PACKAGE_CFLAGS) $(LIBNOTIFY_CFLAGS)
libnotify_a_SOURCES = libnotify.c notification.c notification.h

noinst_LIBRARIES = libnotify.a
endif
