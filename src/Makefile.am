## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = liferea

liferea_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	ui_search.c \
	ui_filter.c \
	ui_tray.c \
	ui_popup.c \
	eggtrayicon.h eggtrayicon.c \
	common.c common.h \
	folder.c folder.h \
	feed.c feed.h \
	item.c item.h \
	export.c export.h \
	filter.c filter.h \
	update.c update.h \
	htmlview.c htmlview.h \
	rss_channel.c rss_channel.h \
	rss_item.c rss_item.h \
	rss_ns.h \
	cdf_channel.c cdf_channel.h \
	cdf_item.c cdf_item.h \
	pie_feed.c pie_feed.h \
	pie_entry.c pie_entry.h \
	pie_ns.h \
	opml.c opml.h \
	ocs_dir.c ocs_dir.h \
	ocs_ns.h \
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
	conf.c conf.h \
	vfolder.c vfolder.h \
	favicon.c favicon.h \
	os-support.c os-support.h \
	netio.c netio.h \
	conversions.c conversions.h

liferea_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

