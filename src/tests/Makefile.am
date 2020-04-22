## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS = html_auto favicon parse_date parse_xml

test: $(TEST_PROGS)
	echo $(TEST_PROGS) |\
	sed "s/^/.\//;s/ / \&\& .\//g" |\
	xargs -I{} sh -c "{}"

memcheck: $(TEST_PROGS)
	./memcheck.sh $(TEST_PROGS)

.PHONY: test

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DBIN_DIR=\""$(bindir)"\" \
	-I$(top_srcdir)/src \
	-I$(top_srcdir) \
	$(PACKAGE_CFLAGS) \
	$(INTROSPECTION_CFLAGS)

favicon_LDADD = \
	../parsers/libliparsers.a \
	../fl_sources/libliflsources.a \
	../ui/libliui.a \
	../webkit/libwebkit.a \
	../auth.o \
	../auth_activatable.o \
	../browser.o \
	../browser_history.o \
	../comments.o \
	../common.o \
	../conf.o \
	../date.o \
	../db.o \
	../dbus.o \
	../debug.o \
	../enclosure.o \
	../export.o \
	../favicon.o \
	../feed.o \
	../feed_parser.o \
	../feedlist.o \
	../folder.o \
	../html.o \
	../htmlview.o \
	../item.o \
	../item_history.o \
	../item_loader.o \
	../item_state.o \
	../itemset.o \
	../itemlist.o \
	../json.o \
	../liferea_application.o \
	../metadata.o \
	../migrate.o \
	../net.o \
	../net_monitor.o \
	../newsbin.o \
	../node.o \
	../node_type.o \
	../node_view.h \
	../plugins_engine.o \
	../render.o \
	../rule.o \
	../social.o \
	../subscription.o \
	../subscription_icon.o \
	../subscription_type.h \
	../update.o \
	../vfolder.o \
	../vfolder_loader.o \
	../xml.o \
	$(PACKAGE_LIBS) \
	$(INTLLIBS) \
	$(WEBKIT_LIBS) \
	$(INTROSPECTION_LIBS) \
	-lm

favicon_SOURCES = favicon.c
favicon_CFLAGS = $(AM_CPPFLAGS)

html_auto_SOURCES = html.c
html_auto_CFLAGS = $(AM_CPPFLAGS)
html_auto_LDADD = $(favicon_LDADD)

parse_date_SOURCES = parse_date.c
parse_date_CFLAGS = $(AM_CPPFLAGS)
parse_date_LDADD = $(favicon_LDADD)

parse_xml_SOURCES = parse_xml.c
parse_xml_CFLAGS = $(AM_CPPFLAGS)
parse_xml_LDADD = $(favicon_LDADD)
