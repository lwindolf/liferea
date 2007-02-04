/**
 * @file gtkhtml2.c GtkHTML2 browser module implementation for Liferea
 *
 * Copyright (C) 2004-2006 Nathan Conrad <conrad@bungled.net>
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>  
 * Copyright (C) 2004 Juho Snellman <jsnell@users.sourceforge.net>
 * 
 * Note large portions of this code (callbacks and html widget
 * preparation) were taken from test/browser-window.c of
 * libgtkhtml-2.2.0 with the following copyrights:
 *
 * Copyright (C) 2000 CodeFactory AB
 * Copyright (C) 2000 Jonas Borgström <jonas@codefactory.se>
 * Copyright (C) 2000 Anders Carlsson <andersca@codefactory.se>
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

#include <libgtkhtml/gtkhtml.h>
#include <libgtkhtml/view/htmlselection.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <errno.h>
#include "common.h"
#include "ui/ui_htmlview.h"
#include "support.h"
#include "callbacks.h"
#include "update.h"
#include "debug.h"

#define BUFFER_SIZE 8192

#define EMPTY "<html><body></body></html>"

/* points to the URL actually under the mouse pointer or is NULL */
static gchar		*selectedURL = NULL;

static GdkCursor	*link_cursor = NULL;

/* prototypes */
static void link_clicked (HtmlDocument *doc, const gchar *url, gpointer data);
static void gtkhtml2_scroll_to_top(GtkWidget *scrollpane);

static int button_press_event(HtmlView *view, GdkEventButton *event, gpointer userdata) {

	g_return_val_if_fail(view != NULL, FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	if((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {
		if(NULL == selectedURL) {
			gtk_menu_popup(GTK_MENU(make_html_menu()), NULL, NULL,
				       NULL, NULL, event->button, event->time);
		} else {
			gdk_window_set_cursor(GDK_WINDOW(gtk_widget_get_parent_window(GTK_WIDGET(view))), NULL);
			gtk_menu_popup(GTK_MENU(make_url_menu(selectedURL)), NULL, NULL,
				       NULL, NULL, event->button, event->time);
		}
		g_free(selectedURL);
		selectedURL = NULL;

		return TRUE; 
	} else {
		return FALSE;
	}
}

/* ------------------------------------------------------------------------------- */
/* GtkHTML Callbacks taken from browser-window.c of libgtkhtml-2.2.0 
   these are needed to automatically resolve links and support formulars
   in the displayed HTML */ 

typedef struct {
	GtkWidget *scrollpane;
	gchar *action;
	gchar *method;
	gchar *encoding;
} SubmitContext;

static int
on_submit_idle(gpointer data)
{
	SubmitContext *ctx = (SubmitContext *)data;
	GtkWidget *htmlwidget = gtk_bin_get_child(GTK_BIN(ctx->scrollpane));
	HtmlDocument *doc = HTML_VIEW(htmlwidget)->document;

	debug3(DEBUG_UPDATE, "action = '%s', method = '%s', encoding = '%s'\n", 
		 ctx->action, ctx->method, ctx->encoding);

	if(ctx->method == NULL || strcasecmp (ctx->method, "get") == 0) {
		gchar *url;
		
		url = g_strdup_printf("%s?%s", ctx->action, ctx->encoding);
		link_clicked(doc, url, ctx->scrollpane );
		g_free (url);
	}
	g_free (ctx);
	return 0;
}

static void
on_submit(HtmlDocument *document, const gchar *action, const gchar *method, 
	   const gchar *encoding, gpointer data)
{
	SubmitContext *ctx = g_new0 (SubmitContext, 1);
	GtkWidget *scrollpane = (GtkWidget*)data;
	
	if(action)
		ctx->action = g_strdup (action);
	if(method)
		ctx->method = g_strdup (method);
	if(action)
		ctx->encoding = g_strdup (encoding);
	ctx->scrollpane = scrollpane;
	
	/* Becase the link_clicked method will clear the document and
	 * start loading a new one, we can't call it directly, because
	 * gtkhtml2 will crash if the document becomes deleted before
	 * this signal handler finish */
	gtk_idle_add(on_submit_idle, ctx);
}


typedef struct {
	HtmlDocument *doc;
	HtmlStream *stream;
} StreamData;

void request_data_kill(struct request *r) {
	GSList *connection_list;
	StreamData *sd = (StreamData*)r->user_data;
	
	html_stream_close(((StreamData*)r->user_data)->stream);
	r->callback = NULL;
	
	connection_list = g_object_get_data(G_OBJECT(sd->doc), "connection_list");
	connection_list = g_slist_remove(connection_list, r);
	g_object_set_data(G_OBJECT(sd->doc), "connection_list", connection_list);
	g_free(r->user_data);
}

static void
stream_cancel (HtmlStream *stream, gpointer user_data, gpointer cancel_data)
{
	struct request *r = (struct request*)cancel_data;

	debug1(DEBUG_UPDATE, "GtkHTML2: Canceling stream: %s", ((struct request*)user_data)->source);
	update_cancel_requests((gpointer)stream);
	
	request_data_kill(r);
}

static void gtkhtml2_url_request_received_cb(struct request *r) {

	if(r->size != 0 && r->data != NULL) {
		html_stream_set_mime_type(((StreamData*)r->user_data)->stream, r->contentType);
		html_stream_write(((StreamData*)r->user_data)->stream, r->data, r->size); 
	}
	request_data_kill(r);
}

static void url_request(HtmlDocument *doc, const gchar *url, HtmlStream *stream, gpointer data) {
	gchar		*absURL;
	
	g_assert(NULL != stream);

	if(NULL != strstr(url, "file://"))
		absURL = g_strdup(url + strlen("file://"));
	else
		absURL = common_build_url(url, g_object_get_data(G_OBJECT(doc), "liferea-base-uri"));

	if(absURL != NULL) {
		struct request *r;
		GSList *connection_list;
		StreamData *sd = g_malloc(sizeof(StreamData));

		sd->doc = doc;
		sd->stream = stream;

		r = update_request_new((gpointer)stream);
		r->options = g_new0(struct updateOptions, 1);
		// FIXME: set no proxy option and HTTP auth properties
		// of selected node to allow viewing of inline images...
		r->source = g_strdup(absURL);
		r->callback = gtkhtml2_url_request_received_cb;
		r->user_data = sd;
		r->priority = 1;
		update_execute_request(r);
		html_stream_set_cancel_func (stream, stream_cancel, r);
		g_free(absURL);

		connection_list = g_object_get_data(G_OBJECT (doc), "connection_list");
		connection_list = g_slist_prepend(connection_list, r);
		g_object_set_data(G_OBJECT (doc), "connection_list", connection_list);
	}
}

static void on_url(HtmlView *view, const char *url, gpointer user_data) {
	xmlChar		*absURL;

	g_free(selectedURL);
	selectedURL = NULL;

	if(NULL != url) {
		gdk_window_set_cursor(GDK_WINDOW(gtk_widget_get_parent_window(GTK_WIDGET(view))), link_cursor);
		absURL = common_build_url(url, g_object_get_data(G_OBJECT(HTML_VIEW(view)->document), "liferea-base-uri"));
		if(absURL != NULL) {
			selectedURL = g_strdup(absURL);
			ui_htmlview_on_url(selectedURL);
			xmlFree(absURL);
		}
	} else {
		gdk_window_set_cursor(GDK_WINDOW(gtk_widget_get_parent_window(GTK_WIDGET(view))), NULL);
		ui_mainwindow_set_status_bar("");
	}
}

static gboolean request_object (HtmlView *view, GtkWidget *widget, gpointer user_data) {

	return FALSE;
}

static void kill_old_connections (GtkWidget *scrollpane) {
	GtkWidget *htmlwidget = gtk_bin_get_child(GTK_BIN(scrollpane));
	HtmlDocument *doc = HTML_VIEW(htmlwidget)->document;
	GSList *connection_list, *tmp;
	struct request *r;
	
	r = g_object_get_data(G_OBJECT(scrollpane), "html_request");
	if (r != NULL)
		r->callback = NULL;
	g_object_set_data(G_OBJECT(scrollpane), "html_request", NULL);
	
	while((tmp = connection_list = g_object_get_data (G_OBJECT (doc), "connection_list")) != NULL)
		request_data_kill((struct request*)tmp->data);
}

static void link_clicked(HtmlDocument *doc, const gchar *url, gpointer scrollpane) {
	xmlChar		*absURL;
	
	absURL = common_build_url(url, g_object_get_data(G_OBJECT(doc), "liferea-base-uri"));
	if(absURL != NULL) {
		kill_old_connections(GTK_WIDGET(scrollpane));
		ui_htmlview_launch_URL(GTK_WIDGET(scrollpane), absURL,
						   GPOINTER_TO_INT(g_object_get_data(G_OBJECT(scrollpane), "internal_browsing")) ?  UI_HTMLVIEW_LAUNCH_INTERNAL: UI_HTMLVIEW_LAUNCH_DEFAULT);
		xmlFree(absURL);
	}
}
void gtkhtml2_destroyed_cb(GtkObject *scrollpane, gpointer user_data) {
	kill_old_connections(GTK_WIDGET(scrollpane));
}

static void gtkhtml2_title_changed(HtmlDocument *doc, const gchar *new_title, gpointer data) {
	ui_tabs_set_title(GTK_WIDGET(data), new_title);
}

/* ---------------------------------------------------------------------------- */
/* Liferea specific code to set up the HTML viewer widget 			*/
/* ---------------------------------------------------------------------------- */

/* adds a differences diff to the actual zoom level */
static void gtkhtml2_change_zoom_level(GtkWidget *scrollpane, gfloat zoomLevel) {
	GtkWidget *htmlwidget = gtk_bin_get_child(GTK_BIN(scrollpane));
	
	/* Clearing the selection is a workaround to avoid 
	   crashes when changing the zoomlevel as reported
	   in SF #1509741 */
	html_selection_clear(HTML_VIEW(htmlwidget));
	
	html_view_set_magnification(HTML_VIEW(htmlwidget), zoomLevel);
}

/* returns the currently set zoom level */
static gfloat gtkhtml2_get_zoom_level(GtkWidget *scrollpane) {
	GtkWidget *htmlwidget = gtk_bin_get_child(GTK_BIN(scrollpane));
	
	return html_view_get_magnification(HTML_VIEW(htmlwidget));
}

/* function to write HTML source given as a UTF-8 string. Note: Originally
   the same doc object was reused over and over. To avoid any problems 
   with this now a new one for each output is created... */
static void gtkhtml2_write_html(GtkWidget *scrollpane, const gchar *string, guint length,  const gchar *base, const gchar *contentType) {
	
	GtkWidget *htmlwidget = gtk_bin_get_child(GTK_BIN(scrollpane));
	HtmlDocument	*doc = HTML_VIEW(htmlwidget)->document;

	/* finalizing older stuff */
	if(NULL != doc) {
		kill_old_connections(scrollpane);
		html_document_clear(doc);	/* heard rumors that this is necessary... */
		if (g_object_get_data(G_OBJECT(doc), "liferea-base-uri") != NULL)
			g_free(g_object_get_data(G_OBJECT(doc), "liferea-base-uri"));
		g_object_unref(G_OBJECT(doc));
	}
	
	doc = html_document_new();
	html_view_set_document(HTML_VIEW(htmlwidget), doc);
	g_object_set_data(G_OBJECT(doc), "liferea-base-uri", g_strdup(base));
	html_document_clear(doc);
	/* Gtkhtml2 only responds to text/html documents, thus everything else must be converted to HTML in Liferea's code */
	html_document_open_stream(doc, "text/html");
	
	g_signal_connect (G_OBJECT (doc), "request_url",
				   GTK_SIGNAL_FUNC (url_request), htmlwidget);
	
	g_signal_connect (G_OBJECT (doc), "submit",
				   GTK_SIGNAL_FUNC (on_submit), scrollpane);
	
	g_signal_connect (G_OBJECT (doc), "link_clicked",
				   G_CALLBACK (link_clicked), scrollpane);

	g_signal_connect (G_OBJECT (doc), "title_changed",
				   G_CALLBACK (gtkhtml2_title_changed), scrollpane);

	if(NULL == string || length == 0)
		html_document_write_stream(doc, EMPTY, strlen(EMPTY));	
	else if (contentType != NULL && !strcmp("text/plain", contentType)) {
		gchar *tmp = g_markup_escape_text(string, length);
		html_document_write_stream(doc, "<html><head></head><body><pre>", strlen("<html><head></head><body><pre>"));
		html_document_write_stream(doc, tmp, strlen(tmp));
		html_document_write_stream(doc, "</pre></body></html>", strlen("</pre></body></html>"));
		g_free(tmp);
	} else {
		html_document_write_stream(doc, string, length);
	}

	
	html_document_close_stream(doc);

	gtkhtml2_change_zoom_level(scrollpane, gtkhtml2_get_zoom_level(scrollpane));	/* to enforce applying of changed zoom levels */
	gtkhtml2_scroll_to_top(scrollpane);
}

static GtkWidget* gtkhtml2_new(gboolean forceInternalBrowsing) {
	gulong	handler;
	GtkWidget *htmlwidget;
	GtkWidget *scrollpane;
	
	link_cursor = gdk_cursor_new(GDK_HAND1);
	scrollpane = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollpane), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrollpane), GTK_SHADOW_IN);
	
	/* create html widget and pack it into the scrolled window */
	htmlwidget = html_view_new();
	gtk_container_add (GTK_CONTAINER (scrollpane), GTK_WIDGET(htmlwidget));
	gtkhtml2_write_html(scrollpane, NULL, 0, "file:///", NULL);
	
	g_object_set_data(G_OBJECT(scrollpane), "internal_browsing", GINT_TO_POINTER(forceInternalBrowsing));
	handler = g_signal_connect(G_OBJECT(htmlwidget), "on_url", G_CALLBACK(on_url), NULL);
	
	/* this is to debug the rare problem reported by some users
	   who get no mouse hovering with GtkHTML2 */
	if(0 == handler)
		g_warning("Could not setup URL handler for GtkHTML2!!!\nPlease help to debug this problem and post a comment on the\nproject homepage including your GTK and GtkHTML2 library versions!\n");
		
	g_signal_connect(G_OBJECT(scrollpane), "destroy", G_CALLBACK(gtkhtml2_destroyed_cb), NULL);
	g_signal_connect(G_OBJECT(htmlwidget), "button-press-event", G_CALLBACK(button_press_event), NULL);
	g_signal_connect(G_OBJECT(htmlwidget), "request_object", G_CALLBACK(request_object), NULL);
	
	gtk_widget_show(htmlwidget);

	return scrollpane;
}



static void gtkhtml2_init() {
}

static void gtkhtml2_deinit() {
}

static void gtkhtml2_html_received(struct request *r) {
	/* Remove reference to the request structure */
	g_object_set_data(G_OBJECT(r->user_data), "html_request", NULL);
	
	/* If no data was returned... */
	if(r->size == 0 || r->data == NULL) {
		/* Maybe an error message should be displayed.... */
		return; /* This should nicely exit.... */
	}
	ui_tabs_set_location(GTK_WIDGET(r->user_data), r->source);
	gtkhtml2_write_html(GTK_WIDGET(r->user_data), r->data, r->size,  r->source, r->contentType);
}

static void gtkhtml2_launch_url(GtkWidget *scrollpane, const gchar *url) { 
	struct request *r;
	
	kill_old_connections(scrollpane);
	
	r = update_request_new(NULL);
	r->source = g_strdup(url);
	r->callback = gtkhtml2_html_received;
	r->user_data = scrollpane;
	r->priority = 1;
	r->options = g_new0(struct updateOptions, 1);
	g_object_set_data(G_OBJECT(scrollpane), "html_request", r);
	update_execute_request(r);
}

static gboolean gtkhtml2_launch_inside_possible(void) { return TRUE; }

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

static struct htmlviewPlugin gtkhtml2Info = {
	HTMLVIEW_PLUGIN_API_VERSION,
	"GtkHTML2",
	gtkhtml2_init,
	gtkhtml2_deinit,
	gtkhtml2_new,
	gtkhtml2_write_html,
	gtkhtml2_launch_url,
	gtkhtml2_launch_inside_possible,
	gtkhtml2_get_zoom_level,
	gtkhtml2_change_zoom_level,
	gtkhtml2_scroll_pagedown,
	/* setProxy = */ NULL,
	/* set_offline_mode */ NULL
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"GtkHTML2 Rendering Plugin",
	PLUGIN_TYPE_HTML_RENDERER,
	&gtkhtml2Info
};

DECLARE_PLUGIN(pi);
DECLARE_HTMLVIEW_PLUGIN(gtkhtml2Info);
