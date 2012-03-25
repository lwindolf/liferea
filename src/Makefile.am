## Process this file with automake to produce Makefile.in

SUBDIRS = parsers notification ui fl_sources webkit .

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DBIN_DIR=\""$(bindir)"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(SM_CFLAGS)

bin_PROGRAMS = liferea
bin_SCRIPTS = liferea-add-feed

liferea_SOURCES = \
	browser.c browser.h \
	comments.c comments.h \
	common.c common.h \
	conf.c conf.h \
	date.c date.h \
	db.c db.h \
	dbus.c dbus.h \
	debug.c debug.h \
	e-date.c e-date.h \
	enclosure.c enclosure.h \
	export.c export.h \
	favicon.c favicon.h \
	feed.c feed.h \
	feed_parser.c feed_parser.h \
	feedlist.c feedlist.h \
	folder.c folder.h \
	html.c html.h \
	htmlview.c htmlview.h \
	item.c item.h \
	item_history.c item_history.h \
	item_loader.c item_loader.h \
	item_state.c item_state.h \
	itemset.c itemset.h \
	itemlist.c itemlist.h \
	json.c json.h \
	metadata.c metadata.h \
	migrate.c migrate.h \
	net.c net.h \
	net_monitor.c net_monitor.h \
	newsbin.c newsbin.h \
	node.c node.h \
	node_type.c node_type.h \
	node_view.h \
	render.c render.h \
	rule.c rule.h \
	social.c social.h \
	subscription.c subscription.h \
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
		$(SYNC_LIB) \
		$(PACKAGE_LIBS) $(SM_LIBS) \
		$(INTLLIBS) $(AVAHI_LIBS) \
		$(WEBKIT_LIBS) $(LIBNOTIFY_LIBS)

EXTRA_DIST = $(srcdir)/liferea-add-feed.in
DISTCLEANFILES = $(srcdir)/liferea-add-feed
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = liferea-add-feed

if WITH_LIBNOTIFY

liferea_LDADD += notification/libnotify.a $(LIBNOTIFY_LIBS)

endif

if WITH_LIBINDICATE

liferea_LDADD += $(LIBINDICATE_LIBS)

endif
