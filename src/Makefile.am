## Process this file with automake to produce Makefile.in

if WITH_GTKHTML2
GTKHTML2 = gtkhtml2
endif

SUBDIRS = net parsers notification ui fl_sources . $(GTKHTML2) mozilla

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DBIN_DIR=\""$(bindir)"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(X_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GNUTLS_CFLAGS)


bin_PROGRAMS = liferea-bin 
bin_SCRIPTS = liferea liferea-add-feed

liferea_bin_SOURCES = \
	callbacks.c callbacks.h \
	common.c common.h \
	conf.c conf.h \
	debug.c debug.h \
	export.c export.h \
	favicon.c favicon.h \
	feed.c feed.h \
	feedlist.c feedlist.h \
	folder.c folder.h \
	html.c html.h \
	interface.c interface.h \
	item.c item.h \
	itemset.c itemset.h \
	itemlist.c itemlist.h \
	main.c \
	metadata.c metadata.h \
	node.c node.h \
	plugin.c plugin.h \
	render.c render.h \
	rule.c rule.h \
	support.c support.h \
	update.c update.h \
	vfolder.c vfolder.h 

liferea_bin_LDADD =	net/liblinet.a \
			parsers/libliparsers.a \
			fl_sources/libliflsources.a \
			notification/liblinotification.a \
			ui/libliui.a \
			$(PACKAGE_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS) $(DBUS_LIBS) $(INTLLIBS)  $(GNUTLS_LIBS)

if PLATFORM_WIN32

liferea_bin_LDFLAGS = -Wl,--export-all-symbols,--out-implib,liferea-bin.a

endif
