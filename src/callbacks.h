/*
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <gtk/gtk.h>
#include <libgtkhtml/gtkhtml.h>

/* constants for attributes in feedstore */
#define FS_TITLE	0
#define FS_STATE	1
#define FS_KEY		2
#define FS_TYPE		3

/* constants for attributes in itemstore */
#define IS_TITLE	0
#define IS_STATE	1
#define IS_PTR		2
#define IS_TIME		3
#define IS_TYPE		4

void initGUI(void);
void updateUI(void);
void print_status(gchar *statustext);
void showInfoBox(gchar *msg);
void showErrorBox(gchar *msg);
void doTrayIcon(void);
void undoTrayIcon(void);
void setTrayToolTip(gchar *string);
void setupTrayIcon(void);
void redrawFeedList(void);

void setupFeedList(GtkWidget *mainview);
void setupItemList(GtkWidget *itemlist);

void on_refreshbtn_clicked(GtkButton *button, gpointer user_data);
void on_popup_refresh_selected(void);

void on_propchangebtn_clicked(GtkButton *button,gpointer user_data);

void on_newbtn_clicked(GtkButton *button, gpointer user_data);
void on_newfeedbtn_clicked(GtkButton *button, gpointer user_data);

void on_propbtn_clicked(GtkButton *button, gpointer user_data);
void on_popup_prop_selected(void);

void on_aboutbtn_clicked(GtkButton *button, gpointer user_data);

void feedlist_selection_changed_cb(GtkTreeSelection *selection,  gpointer data);
void itemlist_selection_changed_cb(GtkTreeSelection *selection, gpointer data);

void
on_newfeedbutton_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_feednamebutton_clicked              (GtkButton       *button,
                                        gpointer         user_data);
					
void on_deletebtn_clicked(GtkButton *button, gpointer user_data);
void on_popup_delete_selected(void);

void on_prefbtn_clicked(GtkButton *button, gpointer user_data);

void
on_prefsavebtn_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_mainfeedlist_button_press_event     (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data);
void on_popup_newfolder_selected(void);

gboolean
on_itemlist_button_press_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_searchbtn_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_hidesearch_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_searchentry_activate                (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_feedlist_drag_end                   (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gpointer         user_data);

void
on_feedlist_drag_begin                 (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gpointer         user_data);

gboolean
on_Itemlist_move_cursor                (GtkTreeView     *treeview,
                                        GtkMovementStep  step,
                                        gint             count,
                                        gpointer         user_data);

gboolean
on_itemlist_button_press_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_foldernamechangebtn_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_newVFolder_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void on_popup_removefolder_selected(void);

gboolean
on_feedlist_drag_drop                  (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
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
on_feedlist_drag_end                   (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gpointer         user_data);

gboolean
on_feedlist_drag_drop                  (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        guint            time,
                                        gpointer         user_data);

void
on_Itemlist_row_activated              (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data);

gboolean
on_quit                                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_itemlist_button_press_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_localfilebtn_pressed                (GtkButton       *button,
                                        gpointer         user_data);

void
on_fileselect_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_localfilebtn_pressed                (GtkButton       *button,
                                        gpointer         user_data);

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

void on_foldername_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_popup_foldername_selected(void);

void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_toggle_condensed_view_selected(void);
			
void on_toggle_item_flag(void);
void on_popup_launchitem_selected(void);
void on_popup_allunread_selected(void);

void on_next_unread_item_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_popup_next_unread_item_selected(void);

void
on_feedsterbtn_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_import_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_export_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_exportfileselect_pressed            (GtkButton       *button,
                                        gpointer         user_data);

void
on_exportfile_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_importfileselect_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_importfile_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_importfile_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_exportfile_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_importfileselect_pressed            (GtkButton       *button,
                                        gpointer         user_data);

void
on_exportfileselect_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_importfileselect_clicked            (GtkButton       *button,
                                        gpointer         user_data);
