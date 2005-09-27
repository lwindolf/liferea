## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(PACKAGE_CFLAGS) $(MOZILLA_CFLAGS) $(DBUS_CFLAGS)

pkglib_LTLIBRARIES = liblihtmlm.la

liblihtmlm_la_SOURCES = \
	mozembed.c \
	mozilla.cpp mozilla.h

liblihtmlm_la_LIBADD = $(PACKAGE_LIBS) $(MOZILLA_LIBS) $(INTLLIBS)
liblihtmlm_la_CFLAGS = $(MOZILLA_CFLAGS) $(PACKAGE_CFLAGS)
liblihtmlm_la_CXXFLAGS = $(MOZILLA_CFLAGS) $(PACKAGE_CFLAGS)

if PLATFORM_WIN32

liblihtmlm_la_LDFLAGS = -module -avoid-version -no-undefined -Wl,$(top_builddir)/src/liferea-bin.a
liblihtmlm_la_DEPENDENCIES = $(top_builddir)/src/liferea-bin$(EXEEXT)

else

liblihtmlm_la_LDFLAGS += -module -avoid-version

endif
