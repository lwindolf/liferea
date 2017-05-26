## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS = html_auto parse_date

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DBIN_DIR=\""$(bindir)"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(INTROSPECTION_CFLAGS)

progs_ldadd =	../parsers/libliparsers.a \
		../fl_sources/libliflsources.a \
		../ui/libliui.a \
		../webkit/libwebkit.a \
		$(PACKAGE_LIBS) \
		$(INTLLIBS) \
		$(WEBKIT_LIBS) \
		$(INTROSPECTION_LIBS)

html_auto_SOURCES = html.c
html_auto_LDADD = $(progs_ldadd) ../html.o ../common.o ../debug.o ../xml.o

parse_date_SOURCES = parse_date.c
parse_date_LDADD = $(progs_ldadd) ../date.o ../common.o ../debug.o ../e-date.o


