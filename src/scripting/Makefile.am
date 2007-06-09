## Process this file with automake to produce Makefile.in

EXTRA_DIST = liferea.i 

INCLUDES = \
        -fno-strict-aliasing \
        -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
        -DPACKAGE_LIB_DIR=\""$(libdir)"\" \
        -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
        -I$(top_srcdir)/src

if WITH_LUA

BUILT_SOURCES = liferea_wrap.h
pkglib_LTLIBRARIES = libliscrlua.la
libliscrlua_la_SOURCES = liferea_wrap.h lua.c ../script.h 
libliscrlua_la_LIBADD = $(PACKAGE_LIBS) $(LUA_LIBS) $(INTLLIBS)
libliscrlua_la_CFLAGS = $(PACKAGE_CFLAGS) $(LUA_CFLAGS)

if PLATFORM_WIN32

libliscrlua_la_LDFLAGS = -avoid-version -module -no-undefined -Wl,$(top_builddir)/src/liferea-bin.a
libliscrlua_la_DEPENDENCIES = liferea.i $(top_builddir)/src/liferea-bin$(EXEEXT)

else

libliscrlua_la_LDFLAGS = -avoid-version -module
libliscrlua_la_DEPENDENCIES = liferea.i

endif

# Swig doesn't support -pthread...
LIFEREA_WRAP_CFLAGS=$(PACKAGE_CFLAGS:-pthread=)

liferea_wrap.h: liferea.i
	swig -lua $(LIFEREA_WRAP_CFLAGS) -o liferea_wrap.h liferea.i
	
endif
