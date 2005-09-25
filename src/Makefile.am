## Process this file with automake to produce Makefile.in

if WITH_GTKHTML2
GTKHTML2 = gtkhtml2
endif

if WITH_MOZILLA
MOZILLA = mozilla
endif

SUBDIRS = net . $(GTKHTML2) $(MOZILLA)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DBIN_DIR=\""$(bindir)"\" \
	$(PACKAGE_CFLAGS) \
	$(X_CFLAGS) \
	$(DBUS_CFLAGS)


bin_PROGRAMS = liferea-bin 
bin_SCRIPTS = liferea

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
	itemlist.c itemlist.h \
	main.c \
	metadata.c metadata.h \
	node.c node.h \
	plugin.c plugin.h \
	rule.c rule.h \
	support.c support.h \
	update.c update.h \
	vfolder.c vfolder.h

liferea_bin_LDADD = net/liblinet.a $(PACKAGE_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS) $(DBUS_LIBS) $(INTLLIBS) 

if PLATFORM_WIN32

liferea_bin_LDFLAGS = -Wl,--export-all-symbols,--out-implib,liferea-bin.a

endif
