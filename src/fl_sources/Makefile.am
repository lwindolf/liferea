## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS)
	
noinst_LIBRARIES = libliflsources.a
libliflsources_a_SOURCES = node_source.c node_source.h \
                           aol_source.c aol_source.h \
			   aol_source_edit.c aol_source_edit.h \
			   aol_source_feed.c \
			   aol_source_opml.c aol_source_opml.h \
                           default_source.c default_source.h \
                           dummy_source.c dummy_source.h \
                           google_source.c google_source.h \
                           json_api_mapper.c json_api_mapper.h \
                           opml_source.c opml_source.h \
                           reedah_source.c reedah_source.h \
			   reedah_source_edit.c reedah_source_edit.h \
			   reedah_source_feed.c \
			   reedah_source_feed_list.c reedah_source_feed_list.h \
                           theoldreader_source.c theoldreader_source.h \
			   theoldreader_source_edit.c theoldreader_source_edit.h \
			   theoldreader_source_feed.c \
			   theoldreader_source_feed_list.c theoldreader_source_feed_list.h \
			   ttrss_source.c ttrss_source.h  \
			   ttrss_source_feed.c \
			   ttrss_source_feed_list.c ttrss_source_feed_list.h 

libliflsources_a_CFLAGS = $(PACKAGE_FLAGS)
