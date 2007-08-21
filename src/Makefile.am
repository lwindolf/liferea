## Process this file with automake to produce Makefile.in

if WITH_GTKHTML2
GTKHTML2 = gtkhtml2
endif

if WITH_AVAHI
SYNC = sync
SYNC_LIB = sync/liblisync.a
endif

SUBDIRS = parsers notification ui fl_sources scripting $(SYNC) . $(GTKHTML2) mozilla

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DBIN_DIR=\""$(bindir)"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(X_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(NM_CFLAGS) \
	$(GNUTLS_CFLAGS) 

bin_PROGRAMS = liferea-bin 
bin_SCRIPTS = liferea liferea-add-feed

liferea_bin_SOURCES = \
	bacon-message-connection.c \
	bacon-message-connection.h \
	comments.c comments.h \
	common.c common.h \
	conf.c conf.h \
	db.c db.h \
	dbus.c dbus.h \
	debug.c debug.h \
	e-date.c e-date.h \
	export.c export.h \
	favicon.c favicon.h \
	feed.c feed.h \
	feedlist.c feedlist.h \
	folder.c folder.h \
	html.c html.h \
	htmlview.c htmlview.h \
	item.c item.h \
	item_state.c item_state.h \
	itemset.c itemset.h \
	itemlist.c itemlist.h \
	itemview.c itemview.h \
	metadata.c metadata.h \
	migrate.c migrate.h \
	net.c net.h \
	newsbin.c newsbin.h \
	node.c node.h \
	node_type.c node_type.h \
	plugin.c plugin.h \
	render.c render.h \
	rule.c rule.h \
	script.c script.h \
	social.c social.h \
	subscription.c subscription.h \
	update.c update.h \
	main.c \
	vfolder.c vfolder.h \
	xml.c xml.h

liferea_bin_LDADD =	parsers/libliparsers.a \
			fl_sources/libliflsources.a \
			notification/liblinotification.a \
			ui/libliui.a \
			$(SYNC_LIB) \
			$(PACKAGE_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS) $(DBUS_LIBS) $(NM_LIBS) $(INTLLIBS) $(GNUTLS_LIBS) $(AVAHI_LIBS)

if WITH_DBUS

EXTRA_DIST = $(srcdir)/liferea_dbus.xml
BUILT_SOURCES = dbus_wrap.c
CLEANFILES = $(BUILT_SOURCES)

dbus_wrap.c: $(srcdir)/liferea_dbus.xml
	dbus-binding-tool --mode=glib-server --prefix=liferea_dbus $(srcdir)/liferea_dbus.xml > dbus_wrap.c

endif

if PLATFORM_WIN32

liferea_bin_LDFLAGS = -Wl,--export-all-symbols,--out-implib,liferea-bin.a

endif
