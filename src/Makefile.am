## Process this file with automake to produce Makefile.in

if WITH_GTKHTML2
GTKHTML2 = gtkhtml2
endif

if WITH_MOZILLA
MOZILLA = mozilla
endif

SUBDIRS = net $(GTKHTML2) $(MOZILLA)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DBIN_DIR=\""$(bindir)"\" \
	@PACKAGE_CFLAGS@ \
	@X_CFLAGS@


##lib_LTLIBRARIES = liblihtmlt.la

##liblihtmlt_la_SOURCES = textview.c
##liblihtmlt_la_LIBADD = @PACKAGE_LIBS@ $(INTLLIBS)

bin_PROGRAMS = liferea-bin 
bin_SCRIPTS = liferea

liferea_bin_SOURCES = \
	callbacks.c callbacks.h \
	cdf_channel.c cdf_channel.h \
	cdf_item.c cdf_item.h \
	common.c common.h \
	conf.c conf.h \
	debug.c debug.h \
	eggtrayicon.h eggtrayicon.c \
	export.c export.h \
	favicon.c favicon.h \
	feed.c feed.h \
	feedlist.c feedlist.h \
	folder.c folder.h \
	guitreemodelfilter.c guitreemodelfilter.h \
	html.c html.h \
	interface.c interface.h \
	item.c item.h \
	itemlist.c itemlist.h \
	main.c \
	metadata.c metadata.h \
	ns_ocs.c ns_ocs.h \
	ns_dc.c ns_dc.h \
	ns_fm.c ns_fm.h \
	ns_slash.c ns_slash.h \
	ns_content.c ns_content.h \
	ns_syn.c ns_syn.h \
	ns_admin.c ns_admin.h \
	ns_blogChannel.c ns_blogChannel.h \
	ns_cC.c ns_cC.h \
	ns_ag.c ns_ag.h \
	ns_photo.c ns_photo.h \
	opml.c opml.h \
	ocs_dir.c ocs_dir.h \
	ocs_ns.h \
	pie_feed.c pie_feed.h \
	pie_entry.c pie_entry.h \
	rss_channel.c rss_channel.h \
	rss_item.c rss_item.h \
	rule.c rule.h \
	support.c support.h \
	ui_dnd.c ui_dnd.h \
	ui_feed.c ui_feed.h \
	ui_feedlist.c ui_feedlist.h \
	ui_folder.c ui_folder.h \
	ui_htmlview.c ui_htmlview.h \
	ui_itemlist.c ui_itemlist.h \
	ui_mainwindow.c ui_mainwindow.h \
	ui_notification.c ui_notification.h \
	ui_popup.c ui_popup.h \
	ui_prefs.c ui_prefs.h \
	ui_queue.c ui_queue.h \
	ui_search.c ui_search.h \
	ui_session.c ui_session.h \
	ui_tray.c ui_tray.h \
	ui_vfolder.c ui_vfolder.h \
	update.c update.h \
	vfolder.c vfolder.h

liferea_bin_LDADD = net/liblinet.a @PACKAGE_LIBS@ @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ $(INTLLIBS)
