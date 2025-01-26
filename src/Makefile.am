## Process this file with automake to produce Makefile.in

SUBDIRS = node_providers node_sources parsers plugins ui webkit tests .

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

js_in_files = $(top_srcdir)/js/item.xml.in \
		$(top_srcdir)/js/node.xml.in
js_files = $(js_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

js_deps = $(top_srcdir)/js/gresource.xml \
	$(top_srcdir)/js/htmlview.js \
	$(top_srcdir)/js/helpers/render.js \
	$(top_srcdir)/js/vendor/Readability.js \
	$(top_srcdir)/js/vendor/purify.min.js \
	$(top_srcdir)/js/vendor/handlebars.min.js \
	$(top_srcdir)/js/i18n-filter.xslt \
	$(top_srcdir)/js/item.xml \
	$(top_srcdir)/js/node.xml

js.h: $(js_files) $(js_deps) $(main_dep)
	glib-compile-resources $(top_srcdir)/js/gresource.xml --generate --internal --target=$@ --c-name js --sourcedir=$(top_srcdir)/js 

js.c: $(js_files) $(js_deps) $(main_dep)
	glib-compile-resources $(top_srcdir)/js/gresource.xml --generate --internal --target=$@ --c-name js --sourcedir=$(top_srcdir)/js 

liferea_SOURCES = \
	auth.c auth.h \
	browser.c browser.h \
	browser_history.c browser_history.h \
	comments.c comments.h \
	common.c common.h \
	conf.c conf.h \
	date.c date.h \
	db.c db.h \
	dbus.c dbus.h \
	debug.c debug.h \
	download.c download.h \
	enclosure.c enclosure.h \
	export.c export.h \
	favicon.c favicon.h \
	feed_parser.c feed_parser.h \
	feedlist.c feedlist.h \
	html.c html.h \
	item.c item.h \
	item_history.c item_history.h \
	item_loader.c item_loader.h \
	item_state.c item_state.h \
	itemset.c itemset.h \
	itemlist.c itemlist.h \
	json.c json.h \
	liferea_application.c liferea_application.h \
	metadata.c metadata.h \
	migrate.c migrate.h \
	net.c net.h \
	net_monitor.c net_monitor.h \
	node.c node.h \
	node_source.c node_source.h \
	node_provider.c node_provider.h \
	node_view.h \
	rule.c rule.h \
	social.c social.h \
	subscription.c subscription.h \
	subscription_icon.c subscription_icon.h \
	subscription_type.h \
	update.c update.h \
	update_job.c update_job.h \
	update_job_queue.c update_job_queue.h \
	main.c \
	vfolder_loader.c vfolder_loader.h \
	xml.c xml.h \
	js.c js.h

liferea_LDADD =	parsers/libliparsers.a \
		node_providers/liblinode_providers.a \
		node_sources/liblinode_sources.a \
		plugins/libliplugins.a \
		ui/libliui.a \
		webkit/libwebkit.a \
		$(PACKAGE_LIBS) \
		$(INTLLIBS) \
		$(WEBKIT_LIBS) \
		$(INTROSPECTION_LIBS) \
		-lm

EXTRA_DIST = $(srcdir)/liferea-add-feed.in $(js_deps) $(js_in_files)
DISTCLEANFILES = $(srcdir)/liferea-add-feed js.c js.h
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = liferea-add-feed

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = Liferea-3.0.gir

Liferea-3.0.gir: liferea$(EXEEXT)
INTROSPECTION_SCANNER_ARGS = -I$(top_srcdir)/src --warn-all --accept-unprefixed --identifier-prefix=Liferea --verbose
Liferea_3_0_gir_NAMESPACE = Liferea
Liferea_3_0_gir_VERSION = 3.0
Liferea_3_0_gir_PROGRAM = $(builddir)/liferea$(EXEEXT)
Liferea_3_0_gir_FILES = auth.c auth.h \
	download.c download.h \
	enclosure.h \
	feedlist.c feedlist.h \
	item.h \
	itemlist.c itemlist.h \
	itemset.c itemset.h \
	liferea_application.h \
	net_monitor.h net_monitor.c \
	node.h node.c \
	node_provider.c node_provider.h \
	node_source.c node_source.h \
	node_view.h \
	social.c social.h \
	subscription_type.h \
	update.h \
	update_job.h \
	update_job_queue.h \
	ui/browser_tabs.c ui/browser_tabs.h \
	ui/icons.c ui/icons.h \
	ui/itemview.c ui/itemview.h \
	ui/item_list_view.c ui/item_list_view.h \
	ui/liferea_browser.c ui/liferea_browser.h \
	ui/liferea_shell.c ui/liferea_shell.h \
	plugins/auth_activatable.c plugins/auth_activatable.h \
	plugins/download_activatable.c plugins/download_activatable.h \
	plugins/liferea_shell_activatable.c plugins/liferea_shell_activatable.h \
	plugins/node_source_activatable.c plugins/node_source_activatable.h

Liferea_3_0_gir_INCLUDES = Gtk-3.0 libxml2-2.0
if HAVE_INTROSPECTION
girdir = $(datadir)/liferea/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/liferea/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = \
	$(gir_DATA)	\
	$(typelib_DATA) \
	js.c js.h
endif
