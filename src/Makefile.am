## Process this file with automake to produce Makefile.in

## I'm totally aware that the module building is 
## not the perfect way to do so :-)

if WITH_GTKHTML
GTKHTML = liblihtmlg.la
GTKHTML_CFLAGS = @GTKHTML_CFLAGS@
endif

if WITH_MOZILLA
MOZILLA = liblihtmlm.la
MOZILLA_CFLAGS = @MOZILLA_CFLAGS@
endif

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ $(GTKHTML_CFLAGS) $(MOZILLA_CFLAGS)


lib_LTLIBRARIES = liblihtmlt.la $(GTKHTML) $(MOZILLA)

liblihtmlt_la_SOURCES = textview.c
liblihtmlt_la_LIBADD = @PACKAGE_LIBS@ $(INTLLIBS)

liblihtmlg_la_SOURCES = gtkhtml.c
liblihtmlg_la_LIBADD = @PACKAGE_LIBS@ @GTKHTML_LIBS@ $(INTLLIBS)

liblihtmlm_la_SOURCES = mozilla.c
liblihtmlm_la_LIBADD = @PACKAGE_LIBS@ @MOZILLA_LIBS@ $(INTLLIBS)

bin_PROGRAMS = liferea

liferea_SOURCES = \
	callbacks.c callbacks.h \
	cdf_channel.c cdf_channel.h \
	cdf_item.c cdf_item.h \
	common.c common.h \
	conversions.c conversions.h \
	conf.c conf.h \
	eggtrayicon.h eggtrayicon.c \
	export.c export.h \
	favicon.c favicon.h \
	feed.c feed.h \
	filter.c filter.h \
	folder.c folder.h \
	htmlview.c htmlview.h \
	interface.c interface.h \
	item.c item.h \
	main.c \
	netio.c netio.h \
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
	opml.c opml.h \
	ocs_dir.c ocs_dir.h \
	ocs_ns.h \
	os-support.c os-support.h \
	pie_feed.c pie_feed.h \
	pie_entry.c pie_entry.h \
	pie_ns.h \
	rss_channel.c rss_channel.h \
	rss_item.c rss_item.h \
	rss_ns.h \
	support.c support.h \
	ui_mainwindow.c ui_mainwindow.h \
	ui_feedlist.c ui_feedlist.h \
	ui_itemlist.c ui_itemlist.h \
	ui_folder.c ui_folder.h \
	ui_search.c ui_search.h \
	ui_filter.c \
	ui_tray.c ui_tray.h \
	ui_popup.c ui_popup.h \
	ui_prefs.c ui_prefs.h \
	ui_dnd.c ui_dnd.h \
	update.c update.h \
	vfolder.c vfolder.h

liferea_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

liferea_CFLAGS = -Wall
