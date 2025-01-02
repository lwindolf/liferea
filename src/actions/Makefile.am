## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src

noinst_LIBRARIES = libliactions.a

libliactions_a_CFLAGS = $(PACKAGE_CFLAGS)
libliactions_a_SOURCES = \
	item_actions.c item_actions.h \
        link_actions.c link_actions.h \
        node_actions.c node_actions.h \
	shell_actions.c shell_actions.h
