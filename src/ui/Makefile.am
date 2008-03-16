## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src

noinst_LIBRARIES = libliui.a

libliui_a_CFLAGS = $(PACKAGE_CFLAGS) $(DBUS_CFLAGS)
libliui_a_SOURCES = \
	eggtrayicon.c eggtrayicon.h \
	rule_editor.c rule_editor.h \
	search_dialog.c search_dialog.h \
	search_engine_dialog.c search_engine_dialog.h \
	search_folder_dialog.c search_folder_dialog.h \
	ui_attention.c ui_attention.h \
	ui_auth.c ui_auth.h \
	ui_dialog.c ui_dialog.h \
	ui_dnd.c ui_dnd.h \
	ui_enclosure.c ui_enclosure.h \
	ui_feedlist.c ui_feedlist.h \
	ui_folder.c ui_folder.h \
	ui_htmlview.c ui_htmlview.h \
	ui_itemlist.c ui_itemlist.h \
	ui_mainwindow.c ui_mainwindow.h \
	ui_node.c ui_node.h \
	ui_popup.c ui_popup.h \
	ui_prefs.c ui_prefs.h \
	ui_script.c ui_script.h \
	ui_search.c ui_search.h \
	ui_session.c ui_session.h \
	ui_shell.c ui_shell.h \
	ui_subscription.c ui_subscription.h \
	ui_tabs.c ui_tabs.h \
	ui_tray.c ui_tray.h \
	ui_update.c ui_update.h 
