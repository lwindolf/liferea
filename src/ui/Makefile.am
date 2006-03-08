## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) \
	$(DBUS_CFLAGS)

noinst_LIBRARIES = libliui.a

libliui_a_CFLAGS = $(PACKAGE_FLAGS)
libliui_a_SOURCES = \
	eggtrayicon.c eggtrayicon.h \
	ui_dnd.c ui_dnd.h \
	ui_enclosure.c ui_enclosure.h \
	ui_feed.c ui_feed.h \
	ui_feedlist.c ui_feedlist.h \
	ui_folder.c ui_folder.h \
	ui_htmlview.c ui_htmlview.h \
	ui_itemlist.c ui_itemlist.h \
	ui_mainwindow.c ui_mainwindow.h \
	ui_node.c ui_node.h \
	ui_notification.c ui_notification.h \
	ui_popup.c ui_popup.h \
	ui_prefs.c ui_prefs.h \
	ui_search.c ui_search.h \
	ui_session.c ui_session.h \
	ui_tabs.c ui_tabs.h \
	ui_tray.c ui_tray.h \
	ui_vfolder.c ui_vfolder.h 
