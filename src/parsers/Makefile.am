## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)

noinst_LIBRARIES = libliparsers.a

libliparsers_a_CFLAGS = $(PACKAGE_FLAGS)
libliparsers_a_SOURCES = \
	atom10.c atom10.h \
	cdf_channel.c cdf_channel.h \
	cdf_item.c cdf_item.h \
	ns_admin.c ns_admin.h \
	ns_ag.c ns_ag.h \
	ns_blogChannel.c ns_blogChannel.h \
	ns_cC.c ns_cC.h \
	ns_content.c ns_content.h \
	ns_dc.c ns_dc.h \
	ns_photo.c ns_photo.h \
	ns_slash.c ns_slash.h \
	ns_syn.c ns_syn.h \
	ns_wfw.c ns_wfw.h \
	pie_entry.c pie_entry.h \
	pie_feed.c pie_feed.h \
	rss_channel.c rss_channel.h \
	rss_item.c rss_item.h 
