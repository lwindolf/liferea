## Process this file with automake to produce Makefile.in

INCLUDES = \
	-fno-strict-aliasing \
        -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
        -DPACKAGE_LIB_DIR=\""$(libdir)"\" \
        -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
        -I$(top_srcdir)/src \
        $(PACKAGE_CFLAGS) \
	$(LUA_CFLAGS)


noinst_LIBRARIES = libliscript.a

libliscript_a_CFLAGS = $(PACKAGE_FLAGS) $(LUA_FLAGS)
libliscript_a_SOURCES = \
	script.c script.h \
	ui_script.c ui_script.h

if WITH_LUA
libliscript_a_DEPENDENCIES = liferea_wrap.h

liferea_wrap.h: liferea.i
	swig -lua $(PACKAGE_CFLAGS) -o liferea_wrap.h liferea.i
endif
