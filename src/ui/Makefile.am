## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LIB_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/src

noinst_LIBRARIES = libliui.a

libliui_a_CFLAGS = $(PACKAGE_CFLAGS) $(LIBINDICATE_CFLAGS)
libliui_a_SOURCES = \
	auth_dialog.c auth_dialog.h \
	browser_tabs.c browser_tabs.h \
	enclosure_list_view.c enclosure_list_view.h \
	feed_list_view.c feed_list_view.h \
	icons.c icons.h \
	item_list_view.c item_list_view.h \
	itemview.c itemview.h \
	liferea_dialog.c liferea_dialog.h \
	liferea_htmlview.c liferea_htmlview.h \
	liferea_shell.c liferea_shell.h \
	liferea_shell_activatable.c liferea_shell_activatable.h \
	popup_menu.c popup_menu.h \
	preferences_dialog.c preferences_dialog.h \
	rule_editor.c rule_editor.h \
	search_dialog.c search_dialog.h \
	search_folder_dialog.c search_folder_dialog.h \
	session.c session.h \
	subscription_dialog.c subscription_dialog.h \
	ui_common.c ui_common.h \
	ui_dnd.c ui_dnd.h \
	ui_folder.c ui_folder.h \
	ui_indicator.c ui_indicator.h \
	ui_node.c ui_node.h \
	ui_tray.c ui_tray.h \
	ui_update.c ui_update.h 
