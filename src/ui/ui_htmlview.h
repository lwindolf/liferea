/**
 * @file ui_htmlview.h  Liferea HTML rendering using rendering plugins
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifndef _UI_HTMLVIEW_H
#define _UI_HTMLVIEW_H

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include "plugin.h"

G_BEGIN_DECLS

#define LIFEREA_HTMLVIEW_TYPE		(liferea_htmlview_get_type ())
#define LIFEREA_HTMLVIEW(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_HTMLVIEW_TYPE, LifereaHtmlView))
#define LIFEREA_HTMLVIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_HTMLVIEW_TYPE, LifereaHtmlViewClass))
#define IS_LIFEREA_HTMLVIEW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_HTMLVIEW_TYPE))
#define IS_LIFEREA_HTMLVIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_HTMLVIEW_TYPE))

typedef struct LifereaHtmlView		LifereaHtmlView;
typedef struct LifereaHtmlViewClass	LifereaHtmlViewClass;
typedef struct LifereaHtmlViewPrivate	LifereaHtmlViewPrivate;

struct LifereaHtmlView
{
	GObject		parent;
	
	/*< private >*/
	LifereaHtmlViewPrivate	*priv;
};

struct LifereaHtmlViewClass 
{
	GtkObjectClass parent_class;
};

GType liferea_htmlview_get_type	(void);

/**
 * Registers an available HTML rendering plugin.
 *
 * @param plugin	plugin info structure
 * @param handle	GModule handle
 *
 * @returns TRUE on success
 */
gboolean liferea_htmlview_plugin_register (pluginPtr plugin, GModule *handle);

/**
 * Releases the effectively used HTML rendering plugin.
 */
void liferea_htmlview_plugin_deregister (void);

/**
 * Performs startup setup of htmlview plugins.
 */
void liferea_htmlview_plugin_init (void);

/** 
 * Function to set up the html view widget for the three
 * and two pane view. 
 *
 * @returns a new Liferea HTML widget
 */
LifereaHtmlView * liferea_htmlview_new (gboolean forceInternalBrowsing);

/**
 * Returns the rendering widget for a HTML view. Only
 * to be used by ui_mainwindow.c for widget reparenting.
 *
 * @param htmlview	the HTML view
 *
 * @returns the rendering widget
 */
GtkWidget *liferea_htmlview_get_widget (LifereaHtmlView *htmlview);

/** 
 * Loads a emtpy HTML page. Resets any item view state.
 *
 * @param htmlview	the HTML view widget to clear
 */
void	liferea_htmlview_clear (LifereaHtmlView *htmlview);

/**
 * Method to display the passed HTML source to the HTML widget.
 *
 * @param htmlview	The htmlview widget to be set
 * @param string	HTML source
 * @param base		base url for resolving relative links
 */
void	liferea_htmlview_write (LifereaHtmlView *htmlview, const gchar *string, const gchar *base);

enum {
	UI_HTMLVIEW_LAUNCH_DEFAULT,
	UI_HTMLVIEW_LAUNCH_EXTERNAL,
	UI_HTMLVIEW_LAUNCH_INTERNAL
};

/**
 * Checks if the passed URL is a special internal Liferea
 * link that should never be handled by the browser. To be
 * used by HTML rendering plugins.
 *
 * @param url		the URL to check
 * @return		TRUE if it is a special URL
 */
gboolean liferea_htmlview_is_special_url (const gchar *url);

/**
 * Callback for plugins to process on-url events. Depending on 
 * the link type the link will be copied to the status bar.
 *
 * @param htmlview	the htmlview causing the event
 * @param url		new URL (or empty string)
 */
void	liferea_htmlview_on_url (LifereaHtmlView *htmlview, const gchar *url);

/**
 * Launches the specified URL in the configured browser or
 * in inside the HTML widget according to the launchType
 * parameter.
 *
 * @param htmlview	the htmlview causing the event
 * @param url		URL to launch
 * @param launchType    Type of launch request: 0 = default, 1 = external, 2 = internal
 */
void	liferea_htmlview_launch_URL (LifereaHtmlView *htmlview, const gchar *url, gint launchType);

/**
 * Function to change the zoom level of the HTML widget.
 * 1.0 is a 1:1 zoom.
 *
 * @param diff	New zoom
 */
void	liferea_htmlview_set_zoom (LifereaHtmlView *htmlview, gfloat zoom);

/**
 * Function to determine the current zoom level.
 *
 * @param htmlview htmlview to examine
 *
 * @return the currently set zoom level 
 */
gfloat	liferea_htmlview_get_zoom (LifereaHtmlView *htmlview);

/**
 * Function to execute the commands needed to open up a URL with the
 * browser specified in the preferences.
 *
 * @param the URI to load
 *
 * @returns TRUE if the URI was opened, or FALSE if there was an error
 */

gboolean liferea_htmlview_launch_in_external_browser (const gchar *uri);

/**
 * Function scrolls down the item views scrolled window.
 *
 * @return FALSE if the scrolled window vertical scroll position is at
 * the maximum and TRUE if the vertical adjustment was increased.
 */
gboolean liferea_htmlview_scroll (void);

/**
 * To be called when HTML view needs to update the proxy settings
 * of the rendering widget implementation.
 */
void liferea_htmlview_update_proxy (void);

/**
 * To be called when HTML view needs to update the online state
 * of the rendering widget implementation.
 *
 * @param online	the new online state
 */
void liferea_htmlview_set_online (gboolean online);

/* glade callbacks */
void on_popup_launch_link_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_copy_url_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_subscribe_url_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_zoomin_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_zoomout_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);

G_END_DECLS

/* interface for HTML rendering plugins */

#define ENCLOSURE_PROTOCOL	"liferea-enclosure://"

#define HTMLVIEW_PLUGIN_API_VERSION 13

typedef struct htmlviewPlugin {
	guint 		api_version;
	char 		*name;			/**< name to be stored in preferences */
	guint		priority;		/**< to allow automatically selecting from multiple available renderers */
	gboolean	externalCss;		/**< TRUE if browser plugin support loading CSS from file */
	
	/* plugin loading and unloading methods */
	void 		(*plugin_init)		(void);
	void 		(*plugin_deinit) 	(void);
	
	GtkWidget*	(*create)		(LifereaHtmlView *htmlview, gboolean forceInternalBrowsing);
	/*void		(*destroy)		(GtkWidget *widget);*/
	void		(*write)		(GtkWidget *widget, const gchar *string, guint length, const gchar *base, const gchar *contentType);
	void		(*launch)		(GtkWidget *widget, const gchar *url);
	gboolean	(*launchInsidePossible)	(void);
	gfloat		(*zoomLevelGet)		(GtkWidget *widget);
	void		(*zoomLevelSet)		(GtkWidget *widget, gfloat zoom);
	gboolean	(*scrollPagedown)	(GtkWidget *widget);
	void		(*setProxy)		(const gchar *hostname, guint port, const gchar *username, const gchar *password);
	void		(*setOffLine)		(gboolean offline);
} *htmlviewPluginPtr;

/* Use this macro to declare a html rendering plugin. */
#define DECLARE_HTMLVIEW_PLUGIN(plugin) \
        G_MODULE_EXPORT htmlviewPluginPtr htmlview_plugin_get_info() { \
                return &plugin; \
        }

#endif
