/*
   This is a browser module implementation using gtkmozembed.
     
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>   
   
   Contains code from the Galeon sources

   Copyright (C) 2000 Marco Pesenti Gritti
 
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <gtkmozembed.h>
#include "../htmlview.h"
#include "../conf.h"
#include "../support.h"
#include "../callbacks.h"
#include "../common.h"
#include "mozilla.h"

/**
 * ContextMenuType: various types of context menu
 */
typedef enum
{
	CONTEXT_NONE     = 0,
	CONTEXT_DEFAULT  = (1 << 1),
	CONTEXT_LINK     = (1 << 2),
	CONTEXT_IMAGE    = (1 << 3),
	CONTEXT_DOCUMENT = (1 << 4),
	CONTEXT_INPUT    = (1 << 5),
	CONTEXT_XUL      = (1 << 7),
} ContextMenuType;

static GtkWidget	*itemView = NULL;
static GtkWidget	*itemListView = NULL;
static GtkWidget	*htmlwidget = NULL;

static gfloat		zoomLevel = 1.0;

/* points to the URL actually under the mouse pointer or is NULL */
static gchar		*selectedURL = NULL;

gchar * getModuleName(void) {
	return g_strdup(_("Mozilla (experimental)"));
}

/* function to write HTML source into the widget */
void writeHTML(const gchar *string) {

	g_assert(NULL != htmlwidget);	
	gtk_moz_embed_open_stream(GTK_MOZ_EMBED(htmlwidget), "file://localhost/", "text/html");

	if((NULL != string) && (strlen(string) > 0))
		gtk_moz_embed_append_data(GTK_MOZ_EMBED(htmlwidget), string, strlen(string));
	else
		gtk_moz_embed_append_data(GTK_MOZ_EMBED(htmlwidget), EMPTY, strlen(EMPTY));	

	gtk_moz_embed_close_stream(GTK_MOZ_EMBED(htmlwidget));
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

	g_print("mozembed_new_window_cb\n");
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
gint mozembed_open_uri_cb (GtkMozEmbed *embed, const char *uri, gpointer data) {
	return ui_htmlview_link_clicked(uri);
}

/* Sets up a html view widget using GtkMozEmbed.
   The signal setting was derived from the Galeon source. */
static void set_html_view(GtkWidget *pane) {
	gchar	*profile;
	int	i;

        /* signals to connect on each embed widget */
	static const struct
	{ 
		char *event; 
		void *func; /* should be a GtkSignalFunc or similar */
	}
	signal_connections[] =
	{
		//{ "location",        mozembed_location_changed_cb  },
		//{ "title",           mozembed_title_changed_cb     },
		//{ "net_start",       mozembed_load_started_cb      },
		//{ "net_stop",        mozembed_load_finished_cb     },
		//{ "net_state_all",   mozembed_net_status_change_cb },
		//{ "progress",        mozembed_progress_change_cb   },
		{ "link_message",    mozembed_link_message_cb      },
		//{ "js_status",       mozembed_js_status_cb         },
		//{ "visibility",      mozembed_visibility_cb        },
		//{ "destroy_browser", mozembed_destroy_brsr_cb      },
		//{ "dom_mouse_down",  mozembed_dom_mouse_down_cb    },	
		{ "dom_mouse_click", mozembed_dom_mouse_click_cb   },
		//{ "dom_key_press",   mozembed_dom_key_press_cb     },
		//{ "size_to",         mozembed_size_to_cb           },
		{ "new_window",      mozembed_new_window_cb        },
		//{ "security_change", mozembed_security_change_cb   },
		{ "open_uri",		 mozembed_open_uri_cb},
		/* terminator -- must be last in the list! */
		{ NULL, NULL } 
	};
	
	if(NULL != htmlwidget)	/* only call once */
		gtk_widget_destroy(htmlwidget);

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

	/* create html widget and pack it into the scrolled window */
	htmlwidget = gtk_moz_embed_new();
	
	/* connect to interesting Mozilla signals */
	for(i = 0; signal_connections[i].event != NULL; i++)
	{
		gtk_signal_connect_while_alive (GTK_OBJECT(htmlwidget),
						signal_connections[i].event,
						signal_connections[i].func, 
						htmlwidget,
						GTK_OBJECT(htmlwidget));
	}

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(pane), htmlwidget);		  
	gtk_widget_show_all(pane);
}

void setHTMLViewMode(gboolean threePane) {
		
	if(FALSE == threePane)
		set_html_view(itemListView);
	else
		set_html_view(itemView);

}

void setupHTMLViews(GtkWidget *pane1, GtkWidget *pane2, gint initialZoomLevel) {

	itemView = pane1;
	itemListView = pane2;
}

/* launches the specified URL */
void launchURL(const gchar *url) {

	gtk_moz_embed_load_url(GTK_MOZ_EMBED(htmlwidget), url); 
}

/* adds a differences diff to the actual zoom level */
void changeZoomLevel(gfloat diff) {

	ui_show_error_box("Sorry, not yet implemented for Mozilla!");
	zoomLevel += diff;
}

/* returns the currently set zoom level */
gfloat getZoomLevel(void) { return zoomLevel; }
