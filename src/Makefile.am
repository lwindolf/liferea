## Process this file with automake to produce Makefile.in

SUBDIRS = parsers ui fl_sources webkit tests .

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DBIN_DIR=\""$(bindir)"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(INTROSPECTION_CFLAGS)

bin_PROGRAMS = liferea
bin_SCRIPTS = liferea-add-feed

liferea_SOURCES = \
	auth.c auth.h \
	auth_activatable.c auth_activatable.h \
	browser.c browser.h \
	browser_history.c browser_history.h \
	comments.c comments.h \
	common.c common.h \
	conf.c conf.h \
	date.c date.h \
	db.c db.h \
	dbus.c dbus.h \
	debug.c debug.h \
	enclosure.c enclosure.h \
	export.c export.h \
	favicon.c favicon.h \
	feed.c feed.h \
	feed_parser.c feed_parser.h \
	feedlist.c feedlist.h \
	folder.c folder.h \
	html.c html.h \
	item.c item.h \
	item_history.c item_history.h \
	item_loader.c item_loader.h \
	item_state.c item_state.h \
	itemset.c itemset.h \
	itemlist.c itemlist.h \
	js.c js.h \
	json.c json.h \
	liferea_application.c liferea_application.h \
	metadata.c metadata.h \
	migrate.c migrate.h \
	net.c net.h \
	net_monitor.c net_monitor.h \
	newsbin.c newsbin.h \
	node.c node.h \
	node_type.c node_type.h \
	node_view.h \
	plugins_engine.c plugins_engine.h \
	render.c render.h \
	rule.c rule.h \
	social.c social.h \
	subscription.c subscription.h \
	subscription_icon.c subscription_icon.h \
	subscription_type.h \
	update.c update.h \
	main.c \
	vfolder.c vfolder.h \
	vfolder_loader.c vfolder_loader.h \
	xml.c xml.h

liferea_LDADD =	parsers/libliparsers.a \
		fl_sources/libliflsources.a \
		ui/libliui.a \
		webkit/libwebkit.a \
		$(PACKAGE_LIBS) \
		$(INTLLIBS) \
		$(WEBKIT_LIBS) \
		$(INTROSPECTION_LIBS) \
		-lm

js.h: $(top_srcdir)/js/gresource.xml $(top_srcdir)/js/htmlview.js $(top_srcdir)/js/Readability-readerable.js $(top_srcdir)/js/Readability.js $(main_dep)
	glib-compile-resources --generate --target=$@ --c-name js --sourcedir=$(top_srcdir)/js $<

js.c: $(top_srcdir)/js/gresource.xml $(top_srcdir)/js/htmlview.js $(top_srcdir)/js/Readability-readerable.js $(top_srcdir)/js/Readability.js $(main_dep)
	glib-compile-resources --generate --target=$@ --c-name js --sourcedir=$(top_srcdir)/js $<

js.o: js.c js.h
main.o: js.h

EXTRA_DIST = $(srcdir)/liferea-add-feed.in
DISTCLEANFILES = $(srcdir)/liferea-add-feed
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = liferea-add-feed

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = Liferea-3.0.gir

Liferea-3.0.gir: liferea$(EXEEXT)
INTROSPECTION_SCANNER_ARGS = -I$(top_srcdir)/src --warn-all --accept-unprefixed --identifier-prefix=Liferea --verbose
Liferea_3_0_gir_NAMESPACE = Liferea
Liferea_3_0_gir_VERSION = 3.0
Liferea_3_0_gir_PROGRAM = $(builddir)/liferea$(EXEEXT)
Liferea_3_0_gir_FILES = \
	auth.c auth.h \
	auth_activatable.c auth_activatable.h \
	enclosure.h \
	feedlist.c feedlist.h \
	item.h \
	itemlist.c itemlist.h \
	itemset.c itemset.h \
	liferea_application.h \
	net_monitor.h net_monitor.c \
	node.h node.c \
	node_view.h \
	social.c social.h \
	subscription_type.h \
	ui/browser_tabs.c ui/browser_tabs.h \
	ui/icons.c ui/icons.h \
	ui/itemview.c ui/itemview.h \
	ui/item_list_view.c ui/item_list_view.h \
	ui/liferea_browser.c ui/liferea_browser.h \
	ui/liferea_shell.c ui/liferea_shell.h \
	ui/liferea_shell_activatable.c ui/liferea_shell_activatable.h \
	ui/media_player.c ui/media_player.h \
	ui/media_player_activatable.c ui/media_player_activatable.h \
	fl_sources/node_source.c fl_sources/node_source.h \
	fl_sources/node_source_activatable.c fl_sources/node_source_activatable.h
Liferea_3_0_gir_INCLUDES = Gtk-3.0 libxml2-2.0
if HAVE_INTROSPECTION
girdir = $(datadir)/liferea/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/liferea/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = \
	$(srcdir)/js.c $(srcdir)/js.h \
	$(gir_DATA)	\
	$(typelib_DATA)
endif
