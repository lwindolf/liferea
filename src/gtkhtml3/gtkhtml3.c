/*
   GtkHTML3 browser module implementation for Liferea

   Copyright (C) 2003,2004 Lars Lindner <lars.lindner@gmx.net>  
   
   Note large portions of this code (callbacks and html widget
   preparation) were taken from testgtkhtml.c of gtkhtml-3.0!
   
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

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkhtml/gtkhtml-properties.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <errno.h>
#include "../htmlview.h"
#include "../conf.h"
#include "../support.h"
#include "../callbacks.h"
#include "../common.h"

#define BUFFER_SIZE 8192

/* declarations and globals for the gtkhtml callbacks */
typedef struct {
	GtkHTML 		*html;
	GtkHTMLStream		*stream;
	GnomeVFSAsyncHandle	*handle;
} StreamData;

static GtkHTML 		*html = NULL;
static GtkHTMLStream	*html_stream_handle = NULL;
static gchar 		*baseURL = NULL;

static gint 		redirect_timerId = 0;
static gchar 		*redirect_url = NULL;

static GtkWidget	*itemView = NULL;
static GtkWidget	*itemListView = NULL;
static GtkWidget	*htmlwidget = NULL;

static gfloat		zoomLevel = 1.0;

/* points to the URL actually under the mouse pointer or is NULL */
static gchar		*selectedURL = NULL;

/* some prototypes */
static void on_link_clicked(GtkHTML *html, const gchar *url, gpointer data);
void launchURL(const gchar *url);
/* ----------------------------------------------------------------------------- */
/* GtkHTML Callbacks taken from testgtkhtml.c of gtkhtml-3.0.10
   these are needed to automatically resolve links and support formulars
   in the displayed HTML 							 */
/* ----------------------------------------------------------------------------- */
static 
int on_button_press_event(GtkWidget *widget, GdkEventButton *event) {

	g_return_val_if_fail (widget != NULL, FALSE);
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

static 
void on_set_base (GtkHTML *html, const gchar *url, gpointer data) {

	if(NULL != baseURL)
		g_free(baseURL);

	if(NULL != html) {
		gtk_html_set_base(html, url);
	} 
		
	baseURL = g_strdup(url);
}

static void
free_stream_data (StreamData *sdata, gboolean remove)
{
	GSList *connection_list;

	if (remove) {
		connection_list = g_object_get_data (G_OBJECT (sdata->html), "connection_list");
		connection_list = g_slist_remove (connection_list, sdata);
		g_object_set_data (G_OBJECT (sdata->html), "connection_list", connection_list);
	}
	g_object_ref (sdata->stream);
	gtk_html_end(sdata->html, sdata->stream, GTK_HTML_STREAM_OK);
	
	g_free (sdata);
}

static void
stream_cancel (GtkHTMLStream *stream, gpointer user_data, gpointer cancel_data)
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
		//free_stream_data (sdata, TRUE);
		g_free (buffer);
	} else {
		gtk_html_write (sdata->html, sdata->stream, buffer, bytes_read);
		
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

static void
on_submit (GtkHTML *html, const gchar *method, const gchar *action, const gchar *encoding, gpointer data) 
{
	GString *tmpstr = g_string_new (action);

	g_print("submitting '%s' to '%s' using method '%s'\n", encoding, action, method);

	if(strcasecmp(method, "GET") == 0) {

		tmpstr = g_string_append_c (tmpstr, '?');
		tmpstr = g_string_append (tmpstr, encoding);
		
		on_link_clicked(html, tmpstr->str, 0);
		
		g_string_free (tmpstr, TRUE);
	} else {
		g_warning ("Unsupported submit method '%s'\n", method);
	}

}

static void
url_requested (GtkHTML *html, const gchar *uri, GtkHTMLStream *stream, gpointer data)
{
	GnomeVFSURI *vfs_base_uri;
	GnomeVFSURI *vfs_uri;
	StreamData *sdata;
	GSList *connection_list;

	if(NULL != baseURL) {
		vfs_base_uri = gnome_vfs_uri_new(baseURL);
		vfs_uri = gnome_vfs_uri_resolve_relative(vfs_base_uri, uri);
		gnome_vfs_uri_unref(vfs_base_uri);
	} else {
		vfs_uri = gnome_vfs_uri_new(uri);
	}

	g_assert (stream != NULL);

	sdata = g_new0 (StreamData, 1);
	sdata->html = html;
	sdata->stream = stream;

	connection_list = g_object_get_data (G_OBJECT (html), "connection_list");
	connection_list = g_slist_prepend (connection_list, sdata);
	g_object_set_data (G_OBJECT (html), "connection_list", connection_list);

	gnome_vfs_async_open_uri (&sdata->handle, vfs_uri, GNOME_VFS_OPEN_READ,
				  GNOME_VFS_PRIORITY_DEFAULT, vfs_open_callback, sdata);

	gnome_vfs_uri_unref (vfs_uri);

	//html_stream_set_cancel_func (stream, stream_cancel, sdata);
}

static void on_url (GtkHTML *html, const char *url, gpointer user_data) {

	if(NULL != url)
		ui_mainwindow_set_status_bar(g_strdup(url));
	else
		ui_mainwindow_set_status_bar(g_strdup(""));

	if(selectedURL)
		g_free(selectedURL);
	selectedURL = g_strdup(url);
}

static void kill_old_connections(GtkHTML *html) {
	GSList *connection_list, *tmp;

	tmp = connection_list = g_object_get_data (G_OBJECT (html), "connection_list");
	while(tmp) {

		StreamData *sdata = (StreamData *)tmp->data;
		gnome_vfs_async_cancel (sdata->handle);
		free_stream_data (sdata, FALSE);

		tmp = tmp->next;
	}
	g_object_set_data (G_OBJECT (html), "connection_list", NULL);
	g_slist_free (connection_list);
}

static void on_link_clicked(GtkHTML *html, const gchar *url, gpointer data) {
	if (ui_htmlview_link_clicked(url) == FALSE) {
		launchURL(url);
	}
}

/* simulate an async object isntantiation */
static int object_timeout(GtkHTMLEmbedded *eb) {
	GtkWidget *w;

/*	w = gtk_check_button_new();
	gtk_widget_show(w);

	printf("inserting custom widget after a delay ...\n");
	gtk_html_embedded_set_descent(eb, rand()%8);
	gtk_container_add (GTK_CONTAINER(eb), w);
	gtk_widget_unref (GTK_WIDGET (eb));
*/
	return FALSE;
}

static gboolean object_requested_cmd(GtkHTML *html, GtkHTMLEmbedded *eb, void *data) {
	/* printf("object requested, wiaint a bit before creating it ...\n"); */

	if (strcmp (eb->classid, "mine:NULL") == 0)
		return FALSE;

	gtk_widget_ref (GTK_WIDGET (eb));
	gtk_timeout_add(rand() % 5000 + 1000, (GtkFunction) object_timeout, eb);
	/* object_timeout (eb); */

	return TRUE;
}

/* ---------------------------------------------------------------------------- */
/* Liferea specific code to set up the HTML viewer widget 			*/
/* ---------------------------------------------------------------------------- */

/* launches the specified URL */
void launchURL(const gchar *url) { on_link_clicked(NULL, url, NULL); }

/* adds a differences diff to the actual zoom level */
void changeZoomLevel(gfloat diff) {

	zoomLevel += diff;
	//html_view_set_magnification(HTML_VIEW(htmlwidget), zoomLevel);
}

/* returns the currently set zoom level */
gfloat getZoomLevel(void) { return zoomLevel; }

gchar * getModuleName(void) {
	return g_strdup(_("GtkHTML3 (experimental)"));
}

/* function to write HTML source given as a UTF-8 string. Note: Originally
   the same doc object was reused over and over. To avoid any problems 
   with this now a new one for each output is created... */
void writeHTML(const gchar *string) {

	html_stream_handle = gtk_html_begin_content(html, "text/html; charset=utf-8");
	
	if((NULL != string) && (g_utf8_strlen(string, -1) > 0))
		gtk_html_write(html, html_stream_handle, string, strlen(string));
	else
		gtk_html_write(html, html_stream_handle, EMPTY, strlen(EMPTY));	

	gtk_html_end(html, html_stream_handle, GTK_HTML_STREAM_OK);

	changeZoomLevel(0.0);	/* to enforce applying of changed zoom levels */
}

static void setupHTMLView(GtkWidget *scrolledwindow) {

	g_assert(NULL != scrolledwindow);

	if(NULL != htmlwidget) 
		gtk_widget_destroy(htmlwidget);

	/* create html widget and pack it into the scrolled window */
	htmlwidget = gtk_html_new_from_string(EMPTY, -1);
	html = GTK_HTML(htmlwidget);
	gtk_html_set_allow_frameset(html, TRUE);
	gtk_html_set_editable(GTK_HTML(htmlwidget), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), htmlwidget);
			
	gtk_widget_set_events(htmlwidget, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

	g_signal_connect(html, "url_requested", G_CALLBACK(url_requested), NULL);
	g_signal_connect(html, "on_url", G_CALLBACK(on_url), NULL);
	g_signal_connect(html, "set_base", G_CALLBACK(on_set_base), NULL);
	g_signal_connect(html, "button_press_event", G_CALLBACK(on_button_press_event), NULL);
	g_signal_connect(html, "link_clicked", G_CALLBACK(on_link_clicked), NULL);
	g_signal_connect(html, "submit", G_CALLBACK(on_submit), NULL);
	g_signal_connect(html, "object_requested", G_CALLBACK(object_requested_cmd), NULL);

	gtk_widget_show_all(scrolledwindow);
}

void setHTMLViewMode(gboolean threePane) {

	if(FALSE == threePane)
		setupHTMLView(itemListView);
	else
		setupHTMLView(itemView);

}

void setupHTMLViews(GtkWidget *pane1, GtkWidget *pane2, gint initialZoomLevel) {

	g_assert(NULL != pane1);
	g_assert(NULL != pane2);

	gnome_vfs_init();

	itemView = pane1;
	itemListView = pane2;
	setHTMLViewMode(FALSE);
	if(0 != initialZoomLevel)
		changeZoomLevel(((gfloat)initialZoomLevel)/100 - zoomLevel);
}
