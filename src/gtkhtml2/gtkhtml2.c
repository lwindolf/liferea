/*
   GtkHTML2 browser module implementation for Liferea

   Copyright (C) 2003,2004 Lars Lindner <lars.lindner@gmx.net>  
   Copyright (C) 2004 Juho Snellman <jsnell@users.sourceforge.net>
   
   Note large portions of this code (callbacks and html widget
   preparation) were taken from test/browser-window.c of
   libgtkhtml-2.2.0 with the following copyrights:

   Copyright (C) 2000 CodeFactory AB
   Copyright (C) 2000 Jonas Borgström <jonas@codefactory.se>
   Copyright (C) 2000 Anders Carlsson <andersca@codefactory.se>
   
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

#include <libgtkhtml/gtkhtml.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <errno.h>
#include "../htmlview.h"
#include "../support.h"
#include "../callbacks.h"

#define BUFFER_SIZE 8192

/* declarations and globals for the gtkhtml callbacks */
typedef struct {
	HtmlDocument *doc;
	HtmlStream *stream;
	GnomeVFSAsyncHandle *handle;
} StreamData;

static GnomeVFSURI 	*baseURI = NULL;
static gfloat		zoomLevel = 1.0;

/* points to the URL actually under the mouse pointer or is NULL */
static gchar		*selectedURL = NULL;

/* prototypes */
static void link_clicked (HtmlDocument *doc, const gchar *url, gpointer data);
void launch_url(GtkWidget *widget, const gchar *url);
static void gtkhtml2_scroll_to_top(GtkWidget *scrollpane);

static int button_press_event (HtmlView *html, GdkEventButton *event, gpointer userdata) {

	g_return_val_if_fail (html != NULL, FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {
		if(NULL == selectedURL)
			gtk_menu_popup(GTK_MENU(make_html_menu()), NULL, NULL,
				       NULL, NULL, event->button, event->time);
		else {
			gtk_menu_popup(GTK_MENU(make_url_menu(selectedURL)), NULL, NULL,
				       NULL, NULL, event->button, event->time);
		}
		return TRUE; 
	} else {
		return FALSE;
	}
}

/* ------------------------------------------------------------------------------- */
/* GtkHTML Callbacks taken from browser-window.c of libgtkhtml-2.2.0 
   these are needed to automatically resolve links and support formulars
   in the displayed HTML */ 

static void
free_stream_data (StreamData *sdata, gboolean remove)
{
	GSList *connection_list;

	if (remove) {
		connection_list = g_object_get_data (G_OBJECT (sdata->doc), "connection_list");
		connection_list = g_slist_remove (connection_list, sdata);
		g_object_set_data (G_OBJECT (sdata->doc), "connection_list", connection_list);
	}
	g_object_ref (sdata->stream);
	html_stream_close(sdata->stream);
	
	g_free (sdata);
}

static void
stream_cancel (HtmlStream *stream, gpointer user_data, gpointer cancel_data)
{
	StreamData *sdata = (StreamData *)cancel_data;
	gnome_vfs_async_cancel (sdata->handle);
	free_stream_data (sdata, TRUE);
}

static void
vfs_close_callback (GnomeVFSAsyncHandle *handle,
		GnomeVFSResult result,
		gpointer callback_data)
{
}

static void
vfs_read_callback (GnomeVFSAsyncHandle *handle, GnomeVFSResult result,
               gpointer buffer, GnomeVFSFileSize bytes_requested,
	       GnomeVFSFileSize bytes_read, gpointer callback_data)
{
	StreamData *sdata = (StreamData *)callback_data;

	if (result != GNOME_VFS_OK) {
		gnome_vfs_async_close (handle, vfs_close_callback, sdata);
		free_stream_data (sdata, TRUE);
		g_free (buffer);
	} else {
		html_stream_write (sdata->stream, buffer, bytes_read);
		
		gnome_vfs_async_read (handle, buffer, bytes_requested, 
				      vfs_read_callback, sdata);
	}
}

static void
vfs_open_callback  (GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gpointer callback_data)
{
	StreamData *sdata = (StreamData *)callback_data;
	if (result != GNOME_VFS_OK) {

		g_warning ("Open failed: %s.\n", gnome_vfs_result_to_string (result));
		free_stream_data (sdata, TRUE);
	} else {
		gchar *buffer;

		buffer = g_malloc (BUFFER_SIZE);
		gnome_vfs_async_read (handle, buffer, BUFFER_SIZE, vfs_read_callback, sdata);
	}
}

typedef struct {
	gpointer window;
	gchar *action;
	gchar *method;
	gchar *encoding;
} SubmitContext;

static int
on_submit_idle (gpointer data)
{
	SubmitContext *ctx = (SubmitContext *)data;

	g_print ("action = '%s', method = '%s', encoding = '%s'\n", 
		 ctx->action, ctx->method, ctx->encoding);

	if (ctx->method == NULL || strcasecmp (ctx->method, "get") == 0) {
		gchar *url;

		url = g_strdup_printf ("%s?%s", ctx->action, ctx->encoding);
		link_clicked (NULL, url, ctx->window);
		g_free (url);
	}
	g_free (ctx);
	return 0;
}

static void
on_submit (HtmlDocument *document, const gchar *action, const gchar *method, 
	   const gchar *encoding, gpointer data)
{
	SubmitContext *ctx = g_new0 (SubmitContext, 1);

	if (action)
		ctx->action = g_strdup (action);
	if (method)
		ctx->method = g_strdup (method);
	if (action)
		ctx->encoding = g_strdup (encoding);
	ctx->window = data;

	/* Becase the link_clicked method will clear the document and
	 * start loading a new one, we can't call it directly, because
	 * gtkhtml2 will crash if the document becomes deleted before
	 * this signal handler finish */
	gtk_idle_add (on_submit_idle, ctx);
}

static void
url_request (HtmlDocument *doc, const gchar *uri, HtmlStream *stream, gpointer data)
{
	GnomeVFSURI *vfs_uri;
	StreamData *sdata;
	GSList *connection_list;

	if (baseURI)
		vfs_uri = gnome_vfs_uri_resolve_relative (baseURI, uri);
	else
		vfs_uri = gnome_vfs_uri_new(uri);

	g_assert (HTML_IS_DOCUMENT(doc));
	g_assert (stream != NULL);

	sdata = g_new0 (StreamData, 1);
	sdata->doc = doc;
	sdata->stream = stream;

	connection_list = g_object_get_data (G_OBJECT (doc), "connection_list");
	connection_list = g_slist_prepend (connection_list, sdata);
	g_object_set_data (G_OBJECT (doc), "connection_list", connection_list);

	gnome_vfs_async_open_uri (&sdata->handle, vfs_uri, GNOME_VFS_OPEN_READ,
				  GNOME_VFS_PRIORITY_DEFAULT, vfs_open_callback, sdata);

	gnome_vfs_uri_unref (vfs_uri);

	html_stream_set_cancel_func (stream, stream_cancel, sdata);
}

static void
on_url (HtmlView *view, const char *url, gpointer user_data)
{
	if(NULL != url)
		ui_mainwindow_set_status_bar(url);
	else
		ui_mainwindow_set_status_bar("");

	if(selectedURL)
		g_free(selectedURL);
	selectedURL = g_strdup(url);
}

static gboolean
request_object (HtmlView *view, GtkWidget *widget, gpointer user_data)
{
	GtkWidget *sel;

	sel = gtk_color_selection_new ();
	gtk_widget_show (sel);

	gtk_container_add (GTK_CONTAINER (widget), sel);

	return TRUE;
}

static void
kill_old_connections (HtmlDocument *doc)
{
	GSList *connection_list, *tmp;

	tmp = connection_list = g_object_get_data (G_OBJECT (doc), "connection_list");
	while(tmp) {

		StreamData *sdata = (StreamData *)tmp->data;
		gnome_vfs_async_cancel (sdata->handle);
		free_stream_data (sdata, FALSE);

		tmp = tmp->next;
	}
	g_object_set_data (G_OBJECT (doc), "connection_list", NULL);
	g_slist_free (connection_list);
}

static void link_clicked(HtmlDocument *doc, const gchar *url, gpointer data) {

	if (ui_htmlview_launch_in_external_browser(url) == FALSE) {
		launch_url(NULL, url);
	}
}

/* ---------------------------------------------------------------------------- */
/* Liferea specific code to set up the HTML viewer widget 			*/
/* ---------------------------------------------------------------------------- */

/* adds a differences diff to the actual zoom level */
void change_zoom_level(gfloat diff) {

	zoomLevel += diff;
	//html_view_set_magnification(HTML_VIEW(htmlwidget), zoomLevel);
}

/* returns the currently set zoom level */
gfloat get_zoom_level(void) { return zoomLevel; }

/* function to write HTML source given as a UTF-8 string. Note: Originally
   the same doc object was reused over and over. To avoid any problems 
   with this now a new one for each output is created... */
void write_html(GtkWidget *scrollpane, const gchar *string) {

	/* HTML widget can be used only from GTK thread */	
	if(gnome_vfs_is_primary_thread()) {
		GtkWidget *htmlwidget = gtk_bin_get_child(GTK_BIN(scrollpane));
		HtmlDocument	*doc = HTML_VIEW(htmlwidget)->document;
		/* finalizing older stuff */
		if(NULL != doc) {
			kill_old_connections(doc);
			html_document_clear(doc);	/* heard rumors that this is necessary... */
			g_object_unref(G_OBJECT(doc));
		}
	
		doc = html_document_new();
		html_view_set_document(HTML_VIEW(htmlwidget), doc);
		html_document_clear(doc);
		html_document_open_stream(doc, "text/html");
		
		g_signal_connect (G_OBJECT (doc), "request_url",
				 GTK_SIGNAL_FUNC (url_request), htmlwidget);

		g_signal_connect (G_OBJECT (doc), "submit",
				  GTK_SIGNAL_FUNC (on_submit), NULL);

		g_signal_connect (G_OBJECT (doc), "link_clicked",
				  G_CALLBACK (link_clicked), NULL);

		if((NULL != string) && (strlen(string) > 0))
			html_document_write_stream(doc, string, strlen(string));
		else
			html_document_write_stream(doc, EMPTY, strlen(EMPTY));	

		html_document_close_stream(doc);
		
		change_zoom_level(0.0);	/* to enforce applying of changed zoom levels */
		gtkhtml2_scroll_to_top(scrollpane);
	}
}

static GtkWidget* gtkhtml2_new() {
	gulong	handler;
	GtkWidget *htmlwidget;
	GtkWidget *scrollpane = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollpane), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrollpane), GTK_SHADOW_IN);
	
	/* create html widget and pack it into the scrolled window */
	htmlwidget = html_view_new();
	gtk_container_add (GTK_CONTAINER (scrollpane), GTK_WIDGET(htmlwidget));
	write_html(scrollpane, NULL);
		  				  				  
	handler = g_signal_connect(G_OBJECT(htmlwidget), "on_url", G_CALLBACK(on_url), NULL);
	
	/* this is to debug the rare problem reported by some users
	   who get no mouse hovering with GtkHTML2 */
	if(0 == handler)
		g_warning("Could not setup URL handler for GtkHTML2!!!\nPlease help to debug this problem and post a comment on the\nproject homepage including your GTK and GtkHTML2 library versions!\n");
		
	g_signal_connect(G_OBJECT(htmlwidget), "button-press-event", G_CALLBACK(button_press_event), NULL);
	g_signal_connect(G_OBJECT(htmlwidget), "request_object", G_CALLBACK(request_object), NULL);


	return scrollpane;
}



static void gtkhtml2_init() {
	gnome_vfs_init();
}

void launch_url(GtkWidget *widget, const gchar *url) { g_warning("should never be called!"); link_clicked(NULL, url, NULL); }

gboolean launch_inside_possible(void) { return FALSE; }

/* -------------------------------------------------------------------- */
/* other functions... 							*/
/* -------------------------------------------------------------------- */

/* Resets the horizontal and vertical scrolling of the items HTML view. */
static void gtkhtml2_scroll_to_top(GtkWidget *scrollpane) {
	GtkScrolledWindow	*itemview;
	GtkAdjustment		*adj;

	itemview = GTK_SCROLLED_WINDOW(scrollpane);
	g_assert(NULL != itemview);
	adj = gtk_scrolled_window_get_vadjustment(itemview);
	gtk_adjustment_set_value(adj, 0.0);
	gtk_scrolled_window_set_vadjustment(itemview, adj);
	gtk_adjustment_value_changed(adj);

	adj = gtk_scrolled_window_get_hadjustment(itemview);
	gtk_adjustment_set_value(adj, 0.0);
	gtk_scrolled_window_set_hadjustment(itemview, adj);
	gtk_adjustment_value_changed(adj);
}

/* Function scrolls down the item views scrolled window.
   This function returns FALSE if the scrolled window
   vertical scroll position is at the maximum and TRUE
   if the vertical adjustment was increased. */
static gboolean gtkhtml2_scroll_pagedown(GtkWidget *scrollpane) {
	GtkScrolledWindow	*itemview;
	GtkAdjustment		*vertical_adjustment;
	gdouble			old_value;
	gdouble			new_value;
	gdouble			limit;

	itemview = GTK_SCROLLED_WINDOW(scrollpane);
	g_assert(NULL != itemview);
	vertical_adjustment = gtk_scrolled_window_get_vadjustment(itemview);
	old_value = gtk_adjustment_get_value(vertical_adjustment);
	new_value = old_value + vertical_adjustment->page_increment;
	limit = vertical_adjustment->upper - vertical_adjustment->page_size;
	if(new_value > limit)
		new_value = limit;
	gtk_adjustment_set_value(vertical_adjustment, new_value);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(itemview), vertical_adjustment);
	return (new_value > old_value);
}


static htmlviewPluginInfo gtkhtml2Info = {
	HTMLVIEW_API_VERSION,
	"GtkHTML2",
	gtkhtml2_init,
	gtkhtml2_new,
	write_html,
	launch_url,
	launch_inside_possible,
	get_zoom_level,
	change_zoom_level,
	gtkhtml2_scroll_pagedown
};

DECLARE_HTMLVIEW_PLUGIN(gtkhtml2Info);
