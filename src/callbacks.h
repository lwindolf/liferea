/**
 * @file callbacks.h misc UI stuff
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _CALLBACKS_H
#define _CALLBACKS_H

#include <gtk/gtk.h>
#include "ui_mainwindow.h"
#include "ui_feedlist.h"
#include "ui_itemlist.h"
#include "ui_folder.h"
#include "ui_search.h"
#include "ui_popup.h"
#include "ui_prefs.h"
#include "ui_dnd.h"
#include "export.h"
#include "htmlview.h"

/* icon constants */
enum icons {
	ICON_READ,
	ICON_UNREAD,
	ICON_FLAG,
	ICON_AVAILABLE,
	ICON_UNAVAILABLE,
	ICON_OCS,
	ICON_FOLDER,
	ICON_VFOLDER,
	ICON_EMPTY,
	ICON_ONLINE,
	ICON_OFFLINE,
	ICON_UPDATING,
	MAX_ICONS
};

extern GdkPixbuf *icons[MAX_ICONS];

/** 
 * GUI initialization methods. Sets up dynamically created
 * widgets, load persistent settings and starts cache loading.
 */
void ui_init(gboolean startIconified);

#define ui_redraw_itemlist()	ui_redraw_widget("Itemlist");
#define ui_redraw_feedlist()	ui_redraw_widget("feedlist");

void ui_redraw_widget(gchar *name);

void ui_update(void);
void ui_show_info_box(const char *format, ...);
void ui_show_error_box(const char *format, ...);

void on_nextbtn_clicked(GtkButton *button, gpointer user_data);
void on_refreshbtn_clicked(GtkButton *button, gpointer user_data);

void on_popup_quit(gpointer callback_data, guint callback_action, GtkWidget *widget);

gboolean on_quit(GtkWidget *widget, GdkEvent *event, gpointer user_data);
		
void on_scrolldown_activate(GtkMenuItem *menuitem, gpointer user_data);

/* FIXME: move the following to ui_filter... */
void
on_popup_filter_selected	       (void);

void
on_addrulebtn_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_rulepropbtn_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_ruleupbtn_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_ruledownbtn_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_rulechangedbtn_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_feedlist_drag_data_get              (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data);

void
on_feedlist_drag_data_received         (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data);

void
on_about_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_folder_new                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_feed_new                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_delete                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_properties                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_update                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_folder_delete                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_folder_rename                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_close                               (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_mainwindow_window_state_event       (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

#endif
