## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src $(PACKAGE_CFLAGS) $(DBUS_CFLAGS)

if WITH_MOZILLA
MOZILLA = liblihtmlm.la
endif

if WITH_XULRUNNER
XULRUNNER = liblihtmlx.la
endif

pkglib_LTLIBRARIES = $(MOZILLA) $(XULRUNNER)

## the Mozilla/Firefox support module

liblihtmlm_la_SOURCES = \
	mozembed.c mozembed.h \
	mozsupport.cpp mozsupport.h \
	mozilla.c

liblihtmlm_la_LIBADD = $(PACKAGE_LIBS) $(MOZILLA_LIBS) $(INTLLIBS)
liblihtmlm_la_CFLAGS = $(MOZILLA_CFLAGS) $(PACKAGE_CFLAGS)
liblihtmlm_la_CXXFLAGS = $(MOZILLA_CFLAGS) $(PACKAGE_CFLAGS)

if PLATFORM_WIN32

liblihtmlm_la_LDFLAGS = -module -avoid-version -no-undefined -Wl,$(top_builddir)/src/liferea-bin.a
liblihtmlm_la_DEPENDENCIES = $(top_builddir)/src/liferea-bin$(EXEEXT)

else

liblihtmlm_la_LDFLAGS = -module -avoid-version

endif


## the XulRunner support module

liblihtmlx_la_SOURCES = \
	mozembed.c mozembed.h \
	mozsupport.cpp mozsupport.h \
	xulrunner.c

liblihtmlx_la_LIBADD = $(PACKAGE_LIBS) $(XULRUNNER_LIBS) $(INTLLIBS)
liblihtmlx_la_CFLAGS = $(XULRUNNER_CFLAGS) $(PACKAGE_CFLAGS)  -DXULRUNNER_HOME=\""$(XULRUNNER_HOME)\""
liblihtmlx_la_CXXFLAGS = $(XULRUNNER_CFLAGS) $(PACKAGE_CFLAGS)

if PLATFORM_WIN32

liblihtmlx_la_LDFLAGS = -module -avoid-version -no-undefined -Wl,$(top_builddir)/src/liferea-bin.a
liblihtmlx_la_DEPENDENCIES = $(top_builddir)/src/liferea-bin$(EXEEXT)

else

liblihtmlx_la_LDFLAGS = -module -avoid-version

endif
