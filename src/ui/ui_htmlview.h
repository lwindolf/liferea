/**
 * @file ui_htmlview.h common interface for browser module implementations
 * and module loading functions
 *
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
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

struct displayset {
	gchar *headtable;
	gchar *head;
	gchar *body;
	gchar *foot;
	gchar *foottable;
};

/**
 * @{ Common HTML definitions
 *
 * Used styles should be defined in the
 * liferea.css style sheet! For some important HTML literals
 * like the item and feed description headers the styles 
 * are duplicated here just in case the style sheet is 
 * missing. 
 */

#define EMPTY		"<html><body></body></html>"

#define HTML_NEWLINE	"<br>"

/* RSS feed/item output definitions (some are used by OCS too!) */

#define HEAD_START		"<table cellspacing=\"0\" class=\"itemhead\">"
#define HEAD_LINE		"<tr><td class=\"headleft\"><b>%s</b></td><td class=\"headright\" width=\"100%%\">%s</td></tr>"
#define HEAD_END		"</table>"

#define FEED_FOOT_TABLE_START	"<table class=\"feedfoot\">"
#define FEED_FOOT_FIRSTTD	"<tr class=\"feedfoot\"><td class=\"feedfootname\"><span class=\"feedfootname\">"
#define FEED_FOOT_NEXTTD	"</span></td><td class=\"feedfootvalue\"><span class=\"feedfootvalue\">"
#define FEED_FOOT_LASTTD	"</span></td></tr>"
#define FEED_FOOT_TABLE_END	"</table>"

#define FEED_FOOT_WRITE(buffer, key, value)	if(NULL != value) { \
							addToHTMLBuffer(&(buffer), FEED_FOOT_FIRSTTD); \
							addToHTMLBuffer(&(buffer), (gchar *)key); \
							addToHTMLBuffer(&(buffer), FEED_FOOT_NEXTTD); \
							addToHTMLBuffer(&(buffer), (gchar *)value); \
							addToHTMLBuffer(&(buffer), FEED_FOOT_LASTTD); \
						}

#define	IMG_START	"<img class=\"feed\" src=\""
#define IMG_END		"\"><br>"

#define TECHNORATI_LINK	"<div class=\"technorati\"><a href=\"http://www.technorati.com/cosmos/search.html?url=%s\"><img src=\"%s\" border=0 alt=\"[Technorati]\"></a></div>"

/* OCS direntry output definitions */

#define FORMAT_START	"<table cellspacing=\"0\" class=\"ocsformats\"><tr><td class=\"ocslink\">"
#define FORMAT_LINK	"<b>Format: </b>"
#define FORMAT_LANGUAGE		"</td></tr><tr><td class=\"ocslanguage\">Language: "
#define FORMAT_UPDATEPERIOD	"</td></tr><tr><td class=\"ocsinterval\">Update Period: "
#define FORMAT_UPDATEFREQUENCY	"</td></tr><tr><td class=\"ocsfrequency\">Update Frequency: "
#define FORMAT_CONTENTTYPE	"</td></tr><tr><td class=\"ocscontenttype\">Content Type: "
#define FORMAT_END	"</td></tr></table>"

/* condensed mode shading */

#define SHADED_START	"<div class=\"itemshaded\">"
#define SHADED_END	"</div>"
#define UNSHADED_START	"<div class=\"itemunshaded\">"
#define UNSHADED_END	"</div>"

/* HTTP and parsing error text */

#define UPDATE_ERROR_START	"<table cellspacing=\"0\" class=\"httperror\"><tr><td class=\"httperror\">"
#define HTTP_ERROR_TEXT		_("The last update of this subscription failed!<br><b>HTTP error code %d: %s</b>")

#define PARSE_ERROR_TEXT	_("There were errors while parsing this feed!")
#define PARSE_ERROR_TEXT2	"<span id=\"pdl\" class=\"detaillink\">(<span class=\"showmore\" onclick=\"javascript:document.getElementById('pd').style.visibility='visible';document.getElementById('pd').style.display='block';document.getElementById('pdl').style.visibility='hidden';document.getElementById('pdl').style.display='none';\">%s</span>)</span> <br><span class=\"details\" id='pd'><b>%s</b><script language=\"javascript\" type=\"text/javascript\">document.getElementById('pdl').style.visibility='visible';document.getElementById('pdl').style.display='inline';</script>"	// explicitly no </span> at the end!

#define FILTER_ERROR_TEXT	_("There were errors while filtering this feed!")
#define FILTER_ERROR_TEXT2	"<span id=\"fdl\" class=\"detaillink\">(<span class=\"showmore\" onclick=\"javascript:document.getElementById('fd').style.visibility='visible';document.getElementById('fd').style.display='block';document.getElementById('fdl').style.visibility='hidden';document.getElementById('fdl').style.display='none';\">%s</span>)</span> <br><span class=\"details\" id='fd'><b><pre>%s</pre></b><script language=\"javascript\" type=\"text/javascript\">document.getElementById('fdl').style.visibility='visible';document.getElementById('fdl').style.display='inline';</script></span>"

#define HTTP410_ERROR_TEXT	_("This feed is discontinued. It's no longer available. Liferea won't update it anymore but you can still access the cached headlines.")
#define UPDATE_ERROR_END	"</td></tr></table>"

/*@}*/

#define HTMLVIEW_API_VERSION 6

typedef struct htmlviewPluginInfo_ htmlviewPluginInfo;

struct htmlviewPluginInfo_ {
	unsigned int 	api_version;
	char 		*name;
	
	void 		(*init)			(void);
	void 		(*deinit) 		(void);
	
	GtkWidget*	(*create)		(gboolean forceInternalBrowsing);
	/*void		(*destroy)		(GtkWidget *widget);*/
	void		(*write)		(GtkWidget *widget, const gchar *string, guint length, const gchar *base, const gchar *contentType);
	void		(*launch)		(GtkWidget *widget, const gchar *url);
	gboolean	(*launchInsidePossible)	(void);
	gfloat		(*zoomLevelGet)		(GtkWidget *widget);
	void		(*zoomLevelSet)		(GtkWidget *widget, gfloat zoom);
	gboolean	(*scrollPagedown)	(GtkWidget *widget);
	void (*setProxy) (gchar *hostname, int port, gchar *username, gchar *password);
};


# define DECLARE_HTMLVIEW_PLUGIN(plugininfo) \
        G_MODULE_EXPORT htmlviewPluginInfo* htmlview_plugin_getinfo() { \
                return &plugininfo; \
        }


/** list type to provide a list of available modules for the preferences dialog */
struct browserModule {
	gchar	*description;
	gchar	*libname;
};

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
 * Function to set up the html view widget for the three
 * and two pane view. 
 */
GtkWidget *ui_htmlview_new(gboolean forceInternalBrowsing);

/** loads a emtpy HTML page */
void	ui_htmlview_clear(GtkWidget *htmlview);

/**
 * Function to add HTML source header to create a valid HTML source.
 *
 * @param buffer	pointer to buffer to add the HTML to
 * @param base		base URL of HTML content
 * @param twoPane	TRUE if output is for two pane mode
 */
void	ui_htmlview_start_output(gchar **buffer, const gchar *base, gboolean twoPane);

/**
 * Function to add HTML source footer to create a valid HTML source.
 *
 * @param buffer	pointer to buffer to add the HTML to
 */
void	ui_htmlview_finish_output(gchar **buffer);

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
 * Function alerts the htmlview that the selected proxy has
 * changed.
 *
 * @param hostname proxy hostname, or NULL to disable the proxy
 * @param port proxy port
 * @param username proxy authentication username
 * @param password proxy authentication password
 */
void ui_htmlview_set_proxy(gchar *hostname, int port, gchar *username, gchar *password);

/* interface.c callbacks */
void on_popup_launch_link_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_copy_url_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_subscribe_url_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_zoomin_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_zoomout_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);

#endif
