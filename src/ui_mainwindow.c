/*
   some functions concerning the main window 
   
   Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "conf.h"
#include "support.h"
#include "ui_mainwindow.h"
#include "ui_tray.h"
#include "ui_itemlist.h"
#include "htmlview.h"

GtkWidget 	*mainwindow;

gboolean	itemlist_mode;		/* TRUE means three pane, FALSE means two panes */

void ui_mainwindow_update_toolbar(void) {
	GtkWidget *widget;
	
	if(NULL != (widget = lookup_widget(mainwindow, "toolbar"))) {	
		/* to avoid "locking out" the user */
		if(getBooleanConfValue(DISABLE_MENUBAR) && getBooleanConfValue(DISABLE_TOOLBAR))
			setBooleanConfValue(DISABLE_TOOLBAR, FALSE);
			
		if(getBooleanConfValue(DISABLE_TOOLBAR))
			gtk_widget_hide(widget);
		else
			gtk_widget_show(widget);
	}
}

void ui_mainwindow_update_menubar(void) {
	GtkWidget *widget;
	
	if(NULL != (widget = lookup_widget(mainwindow, "menubar"))) {
		if(getBooleanConfValue(DISABLE_MENUBAR))
			gtk_widget_hide(widget);
		else
			gtk_widget_show(widget);
	}
}

void switchPaneMode(gboolean new_mode) {
	GtkWidget	*w1;
	
	w1 = lookup_widget(mainwindow, "itemtabs");
	if(TRUE == (itemlist_mode = new_mode))
		gtk_notebook_set_current_page(GTK_NOTEBOOK(w1), 0);
	else 
		gtk_notebook_set_current_page(GTK_NOTEBOOK(w1), 1);
}

static void toggle_condensed_view(void) {
	
	switchPaneMode(!itemlist_mode);
	setHTMLViewMode(itemlist_mode);
	displayItemList();
}

void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data) {
 
	if(!itemlist_mode != GTK_CHECK_MENU_ITEM(menuitem)->active)
		toggle_condensed_view();
}
 
void on_popup_toggle_condensed_view(gpointer cb_data, guint cb_action, GtkWidget *item) {
 
	if(!itemlist_mode != GTK_CHECK_MENU_ITEM(item)->active)
		toggle_condensed_view();
}
