/*
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

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
#include "ui_feedlist.h"
#include "ui_itemlist.h"
#include "ui_folder.h"
#include "ui_search.h"
#include "ui_popup.h"
#include "ui_prefs.h"
#include "ui_dnd.h"
#include "export.h"

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

/* icon constants */
#define ICON_READ 		0
#define ICON_UNREAD 		1
#define ICON_FLAG 		2
#define ICON_AVAILABLE		3
#define ICON_UNAVAILABLE	4
#define ICON_OCS		5
#define ICON_FOLDER		6
#define ICON_HELP		7
#define ICON_VFOLDER		8
#define ICON_EMPTY		9
#define MAX_ICONS		10

void initGUI(void);

#define redrawItemList()	redrawWidget("Itemlist");
#define redrawFeedList()	redrawWidget("feedlist");

void redrawWidget(gchar *name);

void updateUI(void);
void print_status(gchar *statustext);
void showInfoBox(gchar *msg);
void showErrorBox(gchar *msg);

gint checkForUpdateResults(gpointer data);

void on_nextbtn_clicked(GtkButton *button, gpointer user_data);
void on_refreshbtn_clicked(GtkButton *button, gpointer user_data);
void on_popup_refresh_selected(void);

gboolean on_quit(GtkWidget *widget, GdkEvent *event, gpointer user_data);
		
void on_next_unread_item_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_popup_next_unread_item_selected(void);
void on_popup_zoomin_selected(void);
void on_popup_zoomout_selected(void);
void on_popup_copy_url_selected(gpointer    callback_data,
						  guint       callback_action,
						  GtkWidget  *widget);
						  
void on_popup_subscribe_url_selected(gpointer    callback_data,
						  guint       callback_action,
						  GtkWidget  *widget);

// FIXME: move the following to ui_filter...
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
