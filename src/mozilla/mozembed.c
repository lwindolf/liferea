/**
 * @file mozembed.c a browser module implementation using gtkmozembed.
 *   
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>   
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include "../ui_htmlview.h"
#include "../conf.h"
#include "../support.h"
#include "../debug.h"
#include "../ui_popup.h"
#include "mozilla.h"

/* points to the URL actually under the mouse pointer or is NULL */
static gchar		*selectedURL = NULL;

/* -------------------------------------------------------------------- */

/* function to write HTML source into the widget */
static void mozilla_write(GtkWidget *widget, const gchar *string, const gchar *base) {

	g_assert(NULL != widget);
	
	if(!GTK_WIDGET_REALIZED(widget)) 
		return;

	if((NULL != string) && (strlen(string) > 0)) {
		/* Because of a bug in Mozilla, sending the entire string at
		   once causes Mozilla to hang. This seems to avoid that
		   end. So many wasted hours finding the problem.... The
		   result is in in the Mozilla Bugzilla. See report
		   245960. Now off to eat dinner hours late because I missed
		   the bus that I needed to use to get home. -Nathan*/
		int left = strlen(string);
		gtk_moz_embed_open_stream(GTK_MOZ_EMBED(widget), base, "text/html");
		while (left > 0) {
			if (left > 4096) {
				gtk_moz_embed_append_data(GTK_MOZ_EMBED(widget), string, 4096);
				string += 4096;
			} else
				gtk_moz_embed_append_data(GTK_MOZ_EMBED(widget), string, left);
			left -=4096;
		}
		gtk_moz_embed_close_stream(GTK_MOZ_EMBED(widget));
	} else
		gtk_moz_embed_render_data(GTK_MOZ_EMBED(widget), EMPTY, strlen(EMPTY), base, "text/html");
		
	mozilla_scroll_to_top(widget);
}

/* -------------------------------------------------------------------- */
/* callbacks for the mozembed widget as found in mozcallbacks.c of	*/
/* the Galeon source 							*/
/* -------------------------------------------------------------------- */

/**
 * mozembed_new_window_cb: GTKMOZEMBED SIGNAL, emitted any time a new 
 * window is requested by the document 
 */
static void mozembed_new_window_cb(GtkMozEmbed *dummy, GtkMozEmbed **retval, guint chrome_mask, gpointer embed) {

	/* The only time we want to react on new window requests
	   is when the user clicks a link that wants to open in
	   a new window. The following check might not be fully
	   correct (e.g. on initial webpage loading when new popups
	   are requested and the user crosses a link)  */
	if(NULL != selectedURL) {
		if(getBooleanConfValue(BROWSE_INSIDE_APPLICATION))
			ui_tabs_new(selectedURL, selectedURL);
		else
			ui_htmlview_launch_in_external_browser(selectedURL);
	}
	*retval = NULL;
}

/**
 * mozembed_link_message_cb: GTKMOZEMBED SIGNAL, emitted when the 
 * link message changes
 */
static void mozembed_link_message_cb(GtkMozEmbed *dummy, gpointer embed) {
	
	g_free(selectedURL);
	if(NULL != (selectedURL = gtk_moz_embed_get_link_message(dummy))) {
		/* overwrite or clear last status line text */
		ui_mainwindow_set_status_bar(g_strdup(selectedURL));
		
		/* mozilla gives us an empty string when no link is selected */
		if(0 == strlen(selectedURL)) {
			g_free(selectedURL);
			selectedURL = NULL;
		}
	}
}

static gint mozembed_dom_key_press_cb (GtkMozEmbed *dummy, gpointer dom_event, gpointer embed) {
	return mozilla_key_press_cb(embed, dom_event);
}

/**
 * mozembed_dom_mouse_click_cb: GTKMOZEMBED SIGNAL, emitted when user 
 * clicks on the document
 */
static gint mozembed_dom_mouse_click_cb (GtkMozEmbed *dummy, gpointer dom_event, gpointer embed) {
	gint	button;

	if(-1 == (button = mozilla_get_mouse_event_button(dom_event))) {
		g_warning("Cannot determine mouse button!\n");
		return FALSE;
	}

	/* do we have a right mouse button click? */
	if(button == 2) {
		if(NULL == selectedURL)
			gtk_menu_popup(GTK_MENU(make_html_menu()), NULL, NULL,
				       NULL, NULL, button, 0);
		else
			gtk_menu_popup(GTK_MENU(make_url_menu(selectedURL)), NULL, NULL,
				       NULL, NULL, button, 0);
	
		return TRUE;
	/* or a middle button click */
	} else if(button == 1) {
		if(NULL != selectedURL) {	
			ui_tabs_new(selectedURL, selectedURL);
			return TRUE;
		} else {
			return FALSE;
		}			
	} else {
		return FALSE;
	}
}

/**
 * mozembed_dom_mouse_click_cb: GTKMOZEMBED SIGNAL, when the document
 * tries to load a new document, for example when someone clicks on a
 * link in a web page. This signal gives the embedder the opportunity
 * to keep the new document from being loaded. The uri argument is the
 * uri that's going to be loaded.
 */
static gint mozembed_open_uri_cb (GtkMozEmbed *embed, const char *uri, gpointer data) {

	if(getBooleanConfValue(BROWSE_INSIDE_APPLICATION)) {
		return FALSE;
	} else {
		ui_htmlview_launch_URL(GTK_WIDGET(data), (gchar *)uri, FALSE);
		return TRUE;
	}
}

/* Sets up a html view widget using GtkMozEmbed.
   The signal setting was derived from the Galeon source. */
static GtkWidget * mozilla_create() {
	GtkWidget	*widget;
	int		i;
	
	/* signals to connect on each embed widget */
	static const struct
	{ 
		char *event; 
		void *func; /* should be a GtkSignalFunc or similar */
	}
	signal_connections[] =
	{
		/*{ "location",        mozembed_location_changed_cb  },*/
		/*{ "title",           mozembed_title_changed_cb     },*/
		/*{ "net_start",       mozembed_load_started_cb      },*/
		/*{ "net_stop",        mozembed_load_finished_cb     },*/
		/*{ "net_state_all",   mozembed_net_status_change_cb },*/
		/*{ "progress",        mozembed_progress_change_cb   },*/
		{ "link_message",    mozembed_link_message_cb      },
		/*{ "js_status",       mozembed_js_status_cb         },*/
		/*{ "visibility",      mozembed_visibility_cb        },*/
		/*{ "destroy_browser", mozembed_destroy_brsr_cb      },*/
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
	
	return widget;
}

static void mozilla_init() {
	gchar	*profile;
	
	/* some GtkMozEmbed initialization taken from embed.c from the Galeon sources */
	
	/* init mozilla home */
	gtk_moz_embed_set_comp_path((char *)g_getenv("MOZILLA_FIVE_HOME"));

	/* set a path for the profile */
	profile = g_build_filename(g_get_home_dir(), ".liferea/mozilla", NULL);

	/* initialize profile */
	gtk_moz_embed_set_profile_path(profile, "liferea");
	g_free(profile);
	
	/* startup done */
	gtk_moz_embed_push_startup();
	
	mozilla_preference_set_boolean("javascript.enabled", !getBooleanConfValue(DISABLE_JAVASCRIPT));
	mozilla_preference_set_boolean("plugin.default_plugin_disabled", FALSE);
	mozilla_preference_set_boolean("xpinstall.enabled", FALSE);
	mozilla_preference_set_boolean("mozilla.widget.raise-on-setfocus", FALSE);
	mozilla_save_prefs();
}

static void mozilla_deinit() {
	gtk_moz_embed_pop_startup();
}

/* launches the specified URL */
static void launch_url(GtkWidget *widget, const gchar *url) {

	gtk_moz_embed_load_url(GTK_MOZ_EMBED(widget), url); 
}

static gboolean launch_inside_possible(void) { return TRUE; }

static void mozilla_set_proxy(gchar *hostname, int port, gchar *username, gchar *password) {
	if(NULL != hostname) {
		debug0(DEBUG_GUI, "setting proxy for Mozilla");
		mozilla_preference_set("network.proxy.http", hostname);
		mozilla_preference_set_int("network.proxy.http_port", port);
		mozilla_preference_set_int("network.proxy.type", 1);
	} else {
		mozilla_preference_set_int("network.proxy.type", 0);
	}
	
	mozilla_save_prefs();
}

static htmlviewPluginInfo mozillaInfo = {
	HTMLVIEW_API_VERSION,
	"Mozilla",
	mozilla_init,
	mozilla_deinit,
	mozilla_create,
	mozilla_write,
	launch_url,
	launch_inside_possible,
	mozilla_get_zoom,
	mozilla_set_zoom,
	mozilla_scroll_pagedown,
	mozilla_set_proxy
};

DECLARE_HTMLVIEW_PLUGIN(mozillaInfo);
