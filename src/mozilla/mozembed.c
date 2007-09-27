/**
 * @file mozembed.c common gtkmozembed handling.
 *   
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>   
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 *
 * Contains code from the Galeon sources
 *
 * Copyright (C) 2000 Marco Pesenti Gritti
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <gtkmozembed.h>

#include "mozilla/mozsupport.h"
#include "mozilla/mozembed.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_popup.h"
#include "ui/ui_tabs.h"

#define EMPTY "<html><body></body></html>"

extern GtkWidget *mainwindow;

/* function to write HTML source into the widget */
void
mozembed_write (GtkWidget *widget, const gchar *string, guint length, 
                const gchar *base, const gchar *contentType)
{
	g_assert (NULL != widget);
	
	if (!GTK_WIDGET_REALIZED (widget)) 
		return;

	/* prevent meta refresh of last document */
	gtk_moz_embed_stop_load (GTK_MOZ_EMBED (widget));
	
	/* always prevent following local links in self-generated HTML */
	g_object_set_data (G_OBJECT (widget), "localDocument", GINT_TO_POINTER (FALSE));	
	g_object_set_data (G_OBJECT (widget), "selectedURL", NULL);
	
	if (DEBUG_VERBOSE & debug_level)
		debug1 (DEBUG_HTML, "mozilla: HTML string >>>%s<<<", string);
	debug0 (DEBUG_HTML, "mozilla: start writing...");
	
	if (string && (length > 0)) {
		/* Because of a bug in Mozilla, sending the entire string at
		   once causes Mozilla to hang. This seems to avoid that
		   end. So many wasted hours finding the problem.... The
		   result is in in the Mozilla Bugzilla. See report
		   245960. Now off to eat dinner hours late because I missed
		   the bus that I needed to use to get home. -Nathan*/
		int left = length;
		gtk_moz_embed_open_stream (GTK_MOZ_EMBED (widget), "file://", contentType ? contentType : "text/html");
		while (left > 0) {
			if (left > 4096) {
				debug1 (DEBUG_HTML, "mozilla: appending 4 kbytes (missing %d)", left-4096);
				gtk_moz_embed_append_data (GTK_MOZ_EMBED (widget), string, 4096);
				string += 4096;
			} else {
				debug1 (DEBUG_HTML, "mozilla: appending remaining %d bytes", left);
				gtk_moz_embed_append_data (GTK_MOZ_EMBED (widget), string, left);
			}
			left -= 4096;
		}
		gtk_moz_embed_close_stream (GTK_MOZ_EMBED (widget));
	} else {
		gtk_moz_embed_render_data (GTK_MOZ_EMBED (widget), EMPTY, strlen (EMPTY), base, "text/html");
	}
	
	debug0 (DEBUG_HTML, "mozilla: writing finished.");
		
	mozsupport_scroll_to_top (widget);
}

/* -------------------------------------------------------------------- */
/* callbacks for the mozembed widget as found in mozcallbacks.c of	*/
/* the Galeon source 							*/
/* -------------------------------------------------------------------- */

/**
 * mozembed_new_window_cb: GTKMOZEMBED SIGNAL, emitted any time a new 
 * window is requested by the document 
 */
static void
mozembed_new_window_cb (GtkMozEmbed *embed, GtkMozEmbed **newEmbed, 
                        guint chrome_mask, gpointer callback_data)
{
	gchar *selectedURL;

	/* The only time we want to react on new window requests
	   is when the user clicks a link that wants to open in
	   a new window. The following check might not be fully
	   correct (e.g. on initial webpage loading when new popups
	   are requested and the user crosses a link)  */
	*newEmbed = NULL;

	selectedURL = g_object_get_data (G_OBJECT (embed), "selectedURL");
	if (selectedURL) {
		if (conf_get_bool_value (BROWSE_INSIDE_APPLICATION))
			*newEmbed = GTK_MOZ_EMBED (liferea_htmlview_get_widget (ui_tabs_new (NULL, NULL, TRUE)));
		else
			liferea_htmlview_launch_in_external_browser (selectedURL);
	}
}

static void
mozembed_title_changed_cb (GtkMozEmbed *embed, gpointer user_data)
{
	gchar *newTitle;
	
	newTitle = gtk_moz_embed_get_title (embed);
	if (newTitle) {
		ui_tabs_set_title (GTK_WIDGET (user_data), newTitle);
		g_free (newTitle);
	}
}

static void
mozembed_location_changed_cb (GtkMozEmbed *embed, gpointer user_data)
{
	gchar *newLocation;
	
	newLocation = gtk_moz_embed_get_location (embed);
	if (newLocation)
		ui_tabs_set_location (GTK_WIDGET (embed), newLocation);
	g_free (newLocation);
}

static void
mozembed_destroy_brsr_cb (GtkMozEmbed *embed, gpointer user_data)
{
	ui_tabs_close_tab (GTK_WIDGET (embed));
}

/**
 * mozembed_link_message_cb: GTKMOZEMBED SIGNAL, emitted when the 
 * link message changes
 */
static void
mozembed_link_message_cb(GtkMozEmbed *embed, gpointer user_data)
{
	LifereaHtmlView	*htmlview;
	gchar		*selectedURL;
	
	htmlview = g_object_get_data (G_OBJECT (embed), "htmlview");
	selectedURL = g_object_get_data (G_OBJECT (embed), "selectedURL");
	g_free(selectedURL);
	
	selectedURL = gtk_moz_embed_get_link_message (embed);
	if (selectedURL) {
		/* overwrite or clear last status line text */
		liferea_htmlview_on_url (htmlview, selectedURL);
		
		/* mozilla gives us an empty string when no link is selected */
		if (0 == strlen (selectedURL)) {
			g_free (selectedURL);
			selectedURL = NULL;
		}
	}
	
	g_object_set_data (G_OBJECT (embed), "selectedURL", selectedURL);
}

static gint
mozembed_dom_key_press_cb (GtkMozEmbed *embed, gpointer dom_event, gpointer user_data)
{
	return mozsupport_key_press_cb ((gpointer)embed, dom_event);
}

/**
 * mozembed_dom_mouse_click_cb: GTKMOZEMBED SIGNAL, emitted when user 
 * clicks on the document
 */
static gint
mozembed_dom_mouse_click_cb (GtkMozEmbed *embed, gpointer dom_event, gpointer user_data) 
{
	gint		button;
	gboolean	isLocalDoc, safeURL = FALSE;
	gchar		*selectedURL;

	if (-1 == (button = mozsupport_get_mouse_event_button (dom_event))) {
		g_warning ("Cannot determine mouse button!\n");
		return FALSE;
	}
	
	/* source document in local filesystem? */
	isLocalDoc = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (GTK_WIDGET (embed)), "localDocument"));

	/* prevent launching local filesystem links */	
	selectedURL = g_object_get_data (G_OBJECT (embed), "selectedURL");
	if(selectedURL)
		safeURL = (NULL == strstr(selectedURL, "file://")) || isLocalDoc;
		
	/* do we have a right mouse button click? */
	if (button == 2) {
		if(!selectedURL)
			gtk_menu_popup (GTK_MENU (make_html_menu ()), NULL, NULL,
				        NULL, NULL, button, 0);
		else
			gtk_menu_popup (GTK_MENU (make_url_menu (safeURL?selectedURL:"")), NULL, NULL,
				        NULL, NULL, button, 0);
	
		return TRUE;
	} else {
		if (!selectedURL)
			return FALSE;	/* should never happen */
			
		if (!safeURL)
			return TRUE;
			
		/* on middle button click */
		if (button == 1) {
			ui_tabs_new (selectedURL, selectedURL, FALSE);
			return TRUE;
		/* on left button click */
		} else {
			return FALSE;
		}
	}
}

/**
 * mozembed_dom_mouse_click_cb: GTKMOZEMBED SIGNAL, when the document
 * tries to load a new document, for example when someone clicks on a
 * link in a web page. This signal gives the embedder the opportunity
 * to keep the new document from being loaded. The uri argument is the
 * uri that's going to be loaded.
 */
static gint 
mozembed_open_uri_cb (GtkMozEmbed *embed, const char *uri, gpointer data) 
{	
	if ((FALSE == liferea_htmlview_is_special_url (uri)) && 
 	    (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (embed), "internal_browsing")) ||
	    getBooleanConfValue (BROWSE_INSIDE_APPLICATION))) {
		return FALSE;
	} else {
		LifereaHtmlView *htmlview = g_object_get_data (G_OBJECT (data), "htmlview");
		liferea_htmlview_launch_URL (htmlview, (gchar *) uri, UI_HTMLVIEW_LAUNCH_DEFAULT);
		return TRUE;
	}
}

/* Sets up a html view widget using GtkMozEmbed.
   The signal setting was derived from the Galeon source. */
GtkWidget *
mozembed_create (LifereaHtmlView *htmlview, gboolean forceInternalBrowsing)
{
	GtkWidget	*widget;
	gchar		*bgColor;
	int		i;
	
	/* signals to connect on each embed widget */
	static const struct
	{ 
		char *event; 
		void *func; /* should be a GtkSignalFunc or similar */
	}
	signal_connections[] =
	{
		{ "location",        mozembed_location_changed_cb  },
		{ "title",           mozembed_title_changed_cb     },
		/*{ "net_start",       mozembed_load_started_cb      },*/
		/*{ "net_stop",        mozembed_load_finished_cb     },*/
		/*{ "net_state_all",   mozembed_net_status_change_cb },*/
		/*{ "progress",        mozembed_progress_change_cb   },*/
		{ "link_message",    mozembed_link_message_cb      },
		/*{ "js_status",       mozembed_js_status_cb         },*/
		/*{ "visibility",      mozembed_visibility_cb        },*/
		{ "destroy_browser", mozembed_destroy_brsr_cb      },
		/*{ "dom_mouse_down",  mozembed_dom_mouse_down_cb    },*/
		{ "dom_mouse_click", mozembed_dom_mouse_click_cb   },
		{ "dom_key_press",   mozembed_dom_key_press_cb     },
		/*{ "size_to",         mozembed_size_to_cb           },*/
		{ "new_window",      mozembed_new_window_cb        },
		/*{ "security_change", mozembed_security_change_cb   },*/
		{ "open_uri",		 mozembed_open_uri_cb},
		/* terminator -- must be last in the list! */
		{ NULL, NULL } 
	};
	
	/* create html widget and pack it into the scrolled window */
	widget = gtk_moz_embed_new();
	
	/* connect to interesting Mozilla signals */
	for(i = 0; signal_connections[i].event != NULL; i++)
	{
		gtk_signal_connect (GTK_OBJECT(widget),
						signal_connections[i].event,
						signal_connections[i].func, 
						widget);
						
	}
	
	g_object_set_data (G_OBJECT (widget), "htmlview", htmlview);
	g_object_set_data (G_OBJECT (widget), "internal_browsing", GINT_TO_POINTER (forceInternalBrowsing));
	
	/* enforce GTK theme background color as document background */	
	bgColor = g_strdup_printf ("#%.2x%.2x%.2x",
	                           mainwindow->style->base[GTK_STATE_NORMAL].red >> 8,
	                           mainwindow->style->base[GTK_STATE_NORMAL].green >> 8,
	                           mainwindow->style->base[GTK_STATE_NORMAL].blue >> 8);

	mozsupport_preference_set ("browser.display.background_color", bgColor);
	g_free (bgColor);
	
	return widget;
}

void
mozembed_init (void)
{
	gchar	*profile;
	
	debug_enter ("mozembed_init");
	
	/* some GtkMozEmbed initialization taken from embed.c from the Galeon sources */
	
	/* init mozilla home */
	g_assert (g_thread_supported ());
	
	/* set a path for the profile */
	profile = g_build_filename (common_get_cache_path (), "mozilla", NULL);

	/* initialize profile */
	gtk_moz_embed_set_profile_path (profile, "liferea");
	g_free (profile);
	
	/* startup done */
	gtk_moz_embed_push_startup ();
	
	mozsupport_preference_set_boolean ("javascript.enabled", !conf_get_bool_value (DISABLE_JAVASCRIPT));
	mozsupport_preference_set_boolean ("plugin.default_plugin_disabled", FALSE);
	mozsupport_preference_set_boolean ("xpinstall.enabled", FALSE);
	mozsupport_preference_set_boolean ("mozilla.widget.raise-on-setfocus", FALSE);

	/* this prevents popup dialogs and gives IE-like HTML error pages instead */
	mozsupport_preference_set_boolean ("browser.xul.error_pages.enabled", TRUE);

	/* prevent typahead finding to allow Liferea keyboard navigation */
	mozsupport_preference_set_boolean ("accessibility.typeaheadfind", FALSE);
	mozsupport_preference_set_boolean ("accessibility.typeaheadfind.autostart", FALSE);
	
	mozsupport_save_prefs ();
	
	debug_exit ("mozembed_init");
}

void
mozembed_deinit (void)
{
	gtk_moz_embed_pop_startup ();
}

/* launches the specified URL */
void
mozembed_launch_url (GtkWidget *widget, const gchar *url)
{
	gboolean isLocalDoc;

	/* determine if launched URL is a local one and set the flag to allow following local links */
	isLocalDoc = (url == strstr(url, "file://"));
	g_object_set_data (G_OBJECT (widget), "localDocument", GINT_TO_POINTER (isLocalDoc));

	gtk_moz_embed_load_url (GTK_MOZ_EMBED (widget), url); 
}

gboolean mozembed_launch_inside_possible (void) { return TRUE; }

void
mozembed_set_proxy (const gchar *hostname,
                    guint port,
                    const gchar *username,
                    const gchar *password)
{
	if (hostname) {
		debug0 (DEBUG_GUI, "setting proxy for Mozilla");
		mozsupport_preference_set ("network.proxy.http", hostname);
		mozsupport_preference_set_int ("network.proxy.http_port", port);
		mozsupport_preference_set_int ("network.proxy.type", 1);
	} else {
		mozsupport_preference_set_int ("network.proxy.type", 0);
	}
	
	mozsupport_save_prefs ();
}
