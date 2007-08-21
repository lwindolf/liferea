## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src  $(PACKAGE_CFLAGS) $(WEBKIT_CFLAGS)

pkglib_LTLIBRARIES = liblihtmlw.la

liblihtmlw_la_SOURCES = webkit.c
liblihtmlw_la_LIBADD = $(PACKAGE_LIBS) $(WEBKIT_LIBS) $(INTLLIBS)
liblihtmlw_la_CFLAGS = $(PACKAGE_CFLAGS) $(WEBKIT_CFLAGS)

if PLATFORM_WIN32

liblihtmlw_la_LDFLAGS = -avoid-version -module -no-undefined -Wl,$(top_builddir)/src/liferea-bin.a
liblihtmlw_la_DEPENDENCIES = $(top_builddir)/src/liferea-bin$(EXEEXT)

else

liblihtmlw_la_LDFLAGS = -avoid-version -module

endif
