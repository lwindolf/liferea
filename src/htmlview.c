/*
   Note large portions of this code (callbacks and html widget
   preparation) were taken from test/browser-window.c of
   libgtkhtml-2.2.0 with the following copyrights:

   Copyright (C) 2000 CodeFactory AB
   Copyright (C) 2000 Jonas Borgström <jonas@codefactory.se>
   Copyright (C) 2000 Anders Carlsson <andersca@codefactory.se>
   
   The rest (the HTML creation) is 
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>   

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <stdlib.h>
#include <errno.h>
#include "htmlview.h"
#include "conf.h"
#include "support.h"
#include "callbacks.h"

#define BUFFER_SIZE 8192

/* declarations and globals for the gtkhtml callbacks */
typedef struct {
	HtmlDocument *doc;
	HtmlStream *stream;
	GnomeVFSAsyncHandle *handle;
} StreamData;

static HtmlDocument	*doc;

static GnomeVFSURI 	*baseURI = NULL;

/* some prototypes */
static void url_requested(HtmlDocument *doc, const gchar *uri, HtmlStream *stream, gpointer data);
static void on_url (HtmlView *view, const char *url, gpointer user_data);
static void on_submit (HtmlDocument *document, const gchar *action, const gchar *method, const gchar *encoding, gpointer data);
static gboolean request_object (HtmlView *view, GtkWidget *widget, gpointer user_data);
static void link_clicked (HtmlDocument *doc, const gchar *url, gpointer data);
static void kill_old_connections (HtmlDocument *doc);

/* does all preparations before outputting HTML */
void startHTMLOutput(void) {

	g_assert(doc != NULL);
	kill_old_connections(doc);
	html_document_clear(doc);
	html_document_open_stream(doc, "text/html");
}

/* function to write HTML source */
void writeHTML(gchar *string) {

	g_assert(doc != NULL);
	if((NULL != string) && (strlen(string) > 0)) 
		html_document_write_stream(doc, string, strlen(string));
}

/* does all postprocessing after HTML output */
void finishHTMLOutput(void) {

	g_assert(doc != NULL);
	html_document_close_stream(doc);
}

/* creates and initializes the GtkHTML widget */
void setupHTMLView(GtkWidget *mainwindow) {
	GtkWidget	*scrolledwindow;
	GtkWidget	*pane;
	GtkWidget	*htmlwidget;
	char testhtml[] = "<html><body></body></html>";	// FIXME
	
	/* prepare HTML widget */
	doc = html_document_new();
	startHTMLOutput();
	writeHTML(testhtml);
	finishHTMLOutput();
	
	/* prepare a scrolled window */
	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	pane = lookup_widget(mainwindow, "rightpane");					
	gtk_paned_pack2(GTK_PANED (pane), scrolledwindow, TRUE, TRUE);
				
	/* create html widget and pack it into the scrolled window */
	htmlwidget = html_view_new();
	html_view_set_document(HTML_VIEW (htmlwidget), doc);
	html_view_set_magnification(HTML_VIEW (htmlwidget), 1.0);	
	gtk_container_add(GTK_CONTAINER(scrolledwindow), htmlwidget);
	
	g_signal_connect (G_OBJECT (doc), "request_url",
			 GTK_SIGNAL_FUNC (url_requested), htmlwidget);	
			 
	g_signal_connect (G_OBJECT (doc), "submit",
			  GTK_SIGNAL_FUNC (on_submit), mainwindow);

	g_signal_connect (G_OBJECT (doc), "link_clicked",
			  G_CALLBACK (link_clicked), mainwindow);
			  				  
	g_signal_connect (G_OBJECT (htmlwidget), "on_url",
			  G_CALLBACK (on_url), lookup_widget(mainwindow, "statusbar"));

	g_signal_connect (G_OBJECT (htmlwidget), "request_object",
			  G_CALLBACK (request_object), NULL);

	gtk_widget_show_all(scrolledwindow);
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
url_requested (HtmlDocument *doc, const gchar *uri, HtmlStream *stream, gpointer data)
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
	GtkWidget *statusbar = GTK_WIDGET(user_data);

	gtk_label_set_text (GTK_LABEL (GTK_STATUSBAR (statusbar)->label), 
			    url);
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

static void link_clicked(HtmlDocument *doc, const gchar *url, gpointer data)
{	GError	*error = NULL;
	gchar	*cmd;
	gchar	*statusline;

	cmd = g_strdup_printf(getStringConfValue(BROWSER_COMMAND), url);
	g_assert(NULL != cmd);
	if(0 != strstr(cmd, "%s")) {
		showErrorBox(_("There is no %s URL place holder in the browser command string you specified in the preferences dialog!!!"));
	}
	
	g_spawn_command_line_async(cmd, &error);
	if((NULL != error) && (0 != error->code)) {
		statusline = g_strdup_printf("browser command failed: %s", error->message);
		g_error_free(error);
	} else	
		statusline = g_strdup_printf("starting: \"%s\"", cmd);
		
	print_status(statusline);
	g_free(cmd);
	g_free(statusline);

}

void launchURL(const gchar *url) {

	link_clicked(NULL, url, NULL);
}
