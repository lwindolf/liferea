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
	common.c common.h \
	backend.c backend.h \
	update.c update.h \
	htmlview.c htmlview.h \
	rss_channel.c rss_channel.h \
	rss_item.c rss_item.h \
	rss_ns.h \
	cdf_channel.c cdf_channel.h \
	cdf_item.c cdf_item.h \
	ocs_dir.c ocs_dir.h \
	ocs_ns.h \
	ns_ocs.c ns_ocs.h \
	ns_dc.c ns_dc.h \
	ns_fm.c ns_fm.h \
	ns_slash.c ns_slash.h \
	ns_content.c ns_content.h \
	conf.c conf.h

liferea_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

