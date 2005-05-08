## Process this file with automake to produce Makefile.in

INCLUDES = $(PACKAGE_CFLAGS) $(MOZILLA_CFLAGS) $(DBUS_CFLAGS)

pkglib_LTLIBRARIES = liblihtmlm.la

liblihtmlm_la_SOURCES = \
	mozembed.c \
	mozilla.cpp mozilla.h

liblihtmlm_la_LIBADD = $(PACKAGE_LIBS) $(MOZILLA_LIBS) $(INTLLIBS)
liblihtmlm_la_CFLAGS = $(MOZILLA_CFLAGS) $(PACKAGE_CFLAGS)
liblihtmlm_la_CXXFLAGS = $(MOZILLA_CFLAGS) $(PACKAGE_CFLAGS)
liblihtmlm_la_LDFLAGS = -module -avoid-version
