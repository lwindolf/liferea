/**
 * @file ui_popup.h browser tabs
 *
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2006 Nathan Conrad <conrad@bungled.net>
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
#include "ui_htmlview.h"

/** 
 * setup of the tab handling 
 */
void ui_tabs_init (void);

/**
 * opens a new tab with the specified URL
 *
 * @param url	URL to be loaded in new tab (can be NULL to do nothing)
 * @param title	title of the tab to be created
 * @param activate Should the new tab be put in the foreground?
 *
 * @returns the newly created HTML view
 */
LifereaHtmlView * ui_tabs_new (const gchar *url, const gchar *title, gboolean activate);

/**
 * makes the headline tab visible 
 */
void ui_tabs_show_headlines (void);

/**
 * Used to determine which HTML view (a tab or the headlines view)
 * is currently visible and can be used to display HTML that
 * is to be loaded
 *
 * @returns HTML view widget
 */
LifereaHtmlView * ui_tabs_get_active_htmlview (void);

/**
 * Closes the given tab widget.
 *
 * @param tab	the widget
 */
void ui_tabs_close_tab (GtkWidget *tab);

/**
 * Sets the given title as the title of the given tab widget.
 * If the title is to long it will be truncated and suffixed
 * with "..."
 *
 * @param tab	the widget
 * @param title	the title
 */
void ui_tabs_set_title (GtkWidget *tab, const gchar *title);

/**
 * Sets the location URI for the given tab widget. This 
 * automatically loads the URI in the rendering widget.
 *
 * @param tab	the widget
 * @param uri	the location URI
 */
void ui_tabs_set_location (GtkWidget *tab, const gchar *uri);

/* popup menu callbacks */

void on_popup_open_link_in_tab_selected (gpointer callback_data, guint callback_action, GtkWidget *widget);

#endif
