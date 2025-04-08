## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS)
	
noinst_LIBRARIES = libliplugins.a
libliplugins_a_SOURCES = \
	auth_activatable.c auth_activatable.h \
	download_activatable.c download_activatable.h \
	liferea_shell_activatable.c liferea_shell_activatable.h \
        node_source_activatable.c node_source_activatable.h \
        plugins_engine.c plugins_engine.h

libliplugins_a_CFLAGS = $(PACKAGE_FLAGS)


