/**
 * @file ui_popup.h browser tabs
 *
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _UI_TABS_H
#define _UI_TABS_H

#include <gtk/gtk.h>
/** 
 * setup of the tab handling 
 */
void ui_tabs_init(void);

/**
 * opens a new tab with the specified URL
 *
 * @param url	URL to be loaded in new tab (can be NULL to do nothing)
 * @param title	title of the tab to be created
 */
void ui_tabs_new(const gchar *url, const gchar *title);

/**
 * makes the headline tab visible 
 */
void ui_tabs_show_headlines(void);

/**
 * used to determine which htmlview (a tab or the headlines view)
 * is currently visible and can be used to display HTML that
 * is to be loaded
 */
GtkWidget * ui_tabs_get_active_htmlview(void);


/* popup menu callbacks */
void on_popup_open_link_in_tab_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);

#endif
