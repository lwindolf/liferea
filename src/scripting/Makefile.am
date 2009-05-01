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
noinst_LIBRARIES = liblua.a
liblua_a_SOURCES = liferea_wrap.h lua.c ../script.h 
liblua_a_CFLAGS = $(PACKAGE_CFLAGS) $(LUA_CFLAGS)

# Swig doesn't support -pthread...
LIFEREA_WRAP_CFLAGS=$(PACKAGE_CFLAGS:-pthread=)

liferea_wrap.h: liferea.i
	swig -lua $(LIFEREA_WRAP_CFLAGS) -o liferea_wrap.h liferea.i
	
endif
