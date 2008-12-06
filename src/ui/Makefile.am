## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src

noinst_LIBRARIES = libliui.a

libliui_a_CFLAGS = $(PACKAGE_CFLAGS) $(DBUS_CFLAGS)
libliui_a_SOURCES = \
	attention_profile_dialog.c \
	attention_profile_dialog.h \
	auth_dialog.c auth_dialog.h \
	browser_tabs.c browser_tabs.h \
	eggtrayicon.c eggtrayicon.h \
	enclosure_list_view.c enclosure_list_view.h \
	itemview.c itemview.h \
	liferea_htmlview.c liferea_htmlview.h \
	liferea_shell.c liferea_shell.h \
	rule_editor.c rule_editor.h \
	search_dialog.c search_dialog.h \
	search_engine_dialog.c search_engine_dialog.h \
	search_folder_dialog.c search_folder_dialog.h \
	ui_common.c ui_common.h \
	ui_dialog.c ui_dialog.h \
	ui_dnd.c ui_dnd.h \
	ui_feedlist.c ui_feedlist.h \
	ui_folder.c ui_folder.h \
	ui_itemlist.c ui_itemlist.h \
	ui_node.c ui_node.h \
	ui_popup.c ui_popup.h \
	ui_prefs.c ui_prefs.h \
	ui_script.c ui_script.h \
	ui_search.c ui_search.h \
	ui_session.c ui_session.h \
	ui_subscription.c ui_subscription.h \
	ui_tray.c ui_tray.h \
	ui_update.c ui_update.h 
