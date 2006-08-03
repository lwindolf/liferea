/**
 * @file ui_htmlview.h common interface for browser module implementations
 * and module loading functions
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _HTMLVIEW_H
#define _HTMLVIEW_H

#include <glib.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include "plugin.h"

struct displayset {
	gchar *headtable;
	gchar *head;
	gchar *body;
	gchar *foot;
	gchar *foottable;
};

//#define EMPTY		"<html><body></body></html>"

#define HTMLVIEW_PLUGIN_API_VERSION 9

typedef struct htmlviewPlugin {
	unsigned int 	api_version;
	char 		*name;
	
	/* plugin loading and unloading methods */
	void 		(*plugin_init)		(void);
	void 		(*plugin_deinit) 	(void);
	
	GtkWidget*	(*create)		(gboolean forceInternalBrowsing);
	/*void		(*destroy)		(GtkWidget *widget);*/
	void		(*write)		(GtkWidget *widget, const gchar *string, guint length, const gchar *base, const gchar *contentType);
	void		(*launch)		(GtkWidget *widget, const gchar *url);
	gboolean	(*launchInsidePossible)	(void);
	gfloat		(*zoomLevelGet)		(GtkWidget *widget);
	void		(*zoomLevelSet)		(GtkWidget *widget, gfloat zoom);
	gboolean	(*scrollPagedown)	(GtkWidget *widget);
	void		(*setProxy)		(gchar *hostname, int port, gchar *username, gchar *password);
	void		(*setOffLine)		(gboolean offline);
} *htmlviewPluginPtr;

/* Use this macro to declare a html rendering plugin. */
#define DECLARE_HTMLVIEW_PLUGIN(plugin) \
        G_MODULE_EXPORT htmlviewPluginPtr htmlview_plugin_get_info() { \
                return &plugin; \
        }

/** 
 * This function searches the html browser module directory
 * for available modules and builds a list to be displayed in
 * the preferences dialog. Furthermore this function tries
 * to load the configured browser module or if this fails
 * one of the other available modules.
 */
void	ui_htmlview_init(void);

/**
 * Close/free any resources that were allocated when ui_htmlview_init
 * was called.
 */
void	ui_htmlview_deinit();

/**
 * Loads a feed list provider plugin.
 *
 * @param plugin	plugin info structure
 * @param handle	GModule handle
 */
void	ui_htmlview_plugin_load(pluginPtr plugin, GModule *handle);

/** 
 * Function to set up the html view widget for the three
 * and two pane view. 
 */
GtkWidget *ui_htmlview_new(gboolean forceInternalBrowsing);

/** 
 * Loads a emtpy HTML page 
 *
 * @param htmlview	the HTML view widget to clear
 */
void	ui_htmlview_clear(GtkWidget *htmlview);

/**
 * Function to add HTML source header to create a valid HTML source.
 *
 * @param buffer	buffer to add the HTML to
 * @param base		base URL of HTML content
 * @param twoPane	TRUE if output is for two pane mode
 */
void	ui_htmlview_start_output(GString *buffer, const gchar *base, gboolean twoPane);

/**
 * Function to add HTML source footer to create a valid HTML source.
 *
 * @param buffer	buffer to add the HTML to
 */
void	ui_htmlview_finish_output(GString *buffer);

/**
 * Method to display the passed HTML source to the HTML widget.
 *
 * @param htmlview	The htmlview widget to be set
 * @param string	HTML source
 * @param base		base url for resolving relative links
 */
void	ui_htmlview_write(GtkWidget *htmlview, const gchar *string, const gchar *base);

enum {
	UI_HTMLVIEW_LAUNCH_DEFAULT,
	UI_HTMLVIEW_LAUNCH_EXTERNAL,
	UI_HTMLVIEW_LAUNCH_INTERNAL
};

/**
 * Checks if the passed URL is a special internal Liferea
 * link that should never be handled by the browser.
 *
 * @param url		the URL to check
 * @return		TRUE if it is a special URL
 */
gboolean ui_htmlview_is_special_url(const gchar *url);

/**
 * Launches the specified URL in the configured browser or
 * in case of Mozilla inside the HTML widget.
 *
 * @param htmlview		The htmlview widget to be set
 * @param url			URL to launch
 * @param launchType     Type of launch request: 0 = default, 1 = external, 2 = internal
 */
void	ui_htmlview_launch_URL(GtkWidget *htmlview, gchar *url, gint launchType);

/**
 * Function to change the zoom level of the HTML widget.
 * 1.0 is a 1:1 zoom.
 *
 * @param diff	New zoom
 */
void	ui_htmlview_set_zoom(GtkWidget *htmlview, gfloat zoom);

/**
 * Function to determine the current zoom level.
 *
 * @param htmlview htmlview to examine
 *
 * @return the currently set zoom level 
 */
gfloat	ui_htmlview_get_zoom(GtkWidget *htmlview);

/**
 * Function to determine the currently selected URL.
 * The string must be freed by the caller.
 *
 * @return currently selected URL string.  
 */
gchar *	ui_htmlview_get_selected_URL(void);

/**
 * Function to execute the commands needed to open up a URL with the
 * browser specified in the preferences.
 *
 * @param the URI to load
 *
 * @returns TRUE if the URI was opened, or FALSE if there was an error
 */

gboolean ui_htmlview_launch_in_external_browser(const gchar *uri);

/**
 * Function scrolls down the item views scrolled window.
 *
 * @return FALSE if the scrolled window vertical scroll position is at
 * the maximum and TRUE if the vertical adjustment was increased.
 */
gboolean ui_htmlview_scroll(void);

/**
 * Callback for proxy setting changes.
 *
 * @param hostname	proxy hostname, or NULL to disable the proxy
 * @param port		proxy port
 * @param username	proxy authentication username
 * @param password	proxy authentication password
 */
void ui_htmlview_set_proxy(gchar *hostname, int port, gchar *username, gchar *password);

/**
 * Callback for online state changes.
 *
 * @param online	the new online state
 */
void ui_htmlview_change_online_status(gboolean online);

/* interface.c callbacks */
void on_popup_launch_link_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_copy_url_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_subscribe_url_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_zoomin_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_zoomout_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);

#endif
