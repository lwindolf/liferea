## Process this file with automake to produce Makefile.in

if WITH_GTKHTML2
GTKHTML2 = gtkhtml2
endif

if WITH_MOZILLA
MOZILLA = mozilla
endif

SUBDIRS = . $(GTKHTML2) $(MOZILLA)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DBIN_DIR=\""$(bindir)"\" \
	-I ./ui \
	-I ./parsers \
	-I ./fl_providers \
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
	vfolder.c vfolder.h \
	parsers/atom10.c parsers/atom10.h \
	parsers/cdf_channel.c parsers/cdf_channel.h \
	parsers/cdf_item.c parsers/cdf_item.h \
	parsers/ns_admin.c parsers/ns_admin.h \
	parsers/ns_ag.c parsers/ns_ag.h \
	parsers/ns_blogChannel.c parsers/ns_blogChannel.h \
	parsers/ns_cC.c parsers/ns_cC.h \
	parsers/ns_content.c parsers/ns_content.h \
	parsers/ns_dc.c parsers/ns_dc.h \
	parsers/ns_fm.c parsers/ns_fm.h \
	parsers/ns_ocs.c parsers/ns_ocs.h \
	parsers/ns_photo.c parsers/ns_photo.h \
	parsers/ns_slash.c parsers/ns_slash.h \
	parsers/ns_syn.c parsers/ns_syn.h \
	parsers/ocs_dir.c parsers/ocs_dir.h \
	parsers/ocs_ns.h \
	parsers/opml.c parsers/opml.h \
	parsers/pie_entry.c parsers/pie_entry.h \
	parsers/pie_feed.c parsers/pie_feed.h \
	parsers/rss_channel.c parsers/rss_channel.h \
	parsers/rss_item.c parsers/rss_item.h \
	net/compat.h \
	net/conversions.c net/conversions.h \
	net/cookies.c net/cookies.h \
	net/digcalc.c net/digcalc.h \
	net/md5.c net/md5.h \
	net/netio.c net/netio.h \
	net-support.c net/net-support.h \
	net/os-support.c net/os-support.h \
	net/zlib_interface.c net/zlib_interface.h \
	net/downloadlib.h

liferea_bin_LDADD =  $(PACKAGE_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS) $(DBUS_LIBS) $(INTLLIBS) 

if PLATFORM_WIN32

liferea_bin_LDFLAGS = -Wl,--export-all-symbols,--out-implib,liferea-bin.a

endif
