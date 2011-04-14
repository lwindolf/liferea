## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS)
	
noinst_LIBRARIES = libliflsources.a
libliflsources_a_SOURCES = node_source.c node_source.h \
                           default_source.c default_source.h \
                           dummy_source.c dummy_source.h \
                           google_source.c google_source.h \
			   google_source_edit.c google_source_edit.h \
			   google_source_feed.c \
			   google_source_opml.c google_source_opml.h \
                           opml_source.c opml_source.h \
			   ttrss_source.c ttrss_source.h  \
			   ttrss_source_feed.c \
			   ttrss_source_feed_list.c ttrss_source_feed_list.h 

libliflsources_a_CFLAGS = $(PACKAGE_FLAGS)
