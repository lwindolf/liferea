## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src  $(PACKAGE_CFLAGS) $(GTKHTML2_CFLAGS) $(DBUS_CFLAGS)

pkglib_LTLIBRARIES = liblihtmlg.la

liblihtmlg_la_SOURCES = gtkhtml2.c
liblihtmlg_la_LIBADD = $(PACKAGE_LIBS) $(GTKHTML2_LIBS) $(INTLLIBS)
liblihtmlg_la_CFLAGS = $(PACKAGE_CFLAGS) $(GTKHTML2_CFLAGS)

if PLATFORM_WIN32

liblihtmlg_la_LDFLAGS = -avoid-version -module -no-undefined -Wl,$(top_builddir)/src/liferea-bin.a
liblihtmlg_la_DEPENDENCIES = $(top_builddir)/src/liferea-bin$(EXEEXT)

else

liblihtmlg_la_LDFLAGS = -avoid-version -module

endif
