/**
 * @file gtkhtml2.c GtkHTML2 browser module implementation for Liferea
 *
 * Copyright (C) 2004-2006 Nathan Conrad <conrad@bungled.net>
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>  
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
#include "debug.h"
#include "itemlist.h"
#include "subscription.h"	// FIXME: move+rename FEED_REQ_PRIORITY_HIGH
#include "update.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_popup.h"
#include "ui/ui_tabs.h"

#define BUFFER_SIZE 8192

#define EMPTY "<html><body></body></html>"

/* points to the URL actually under the mouse pointer or is NULL */
static gchar		*selectedURL = NULL;

static GdkCursor	*link_cursor = NULL;

/* prototypes */
static void link_clicked (HtmlDocument *doc, const gchar *url, gpointer data);
static void gtkhtml2_scroll_to_top(GtkWidget *scrollpane);

static int
button_press_event (HtmlView *view, GdkEventButton *event, gpointer userdata)
{
	gboolean 	safeURL = FALSE;
	gboolean	isLocalDoc = FALSE;
	
	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if ((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {

		/* source document in local filesystem? */
		isLocalDoc = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (view->document), "localDocument"));

		/* prevent launching local filesystem links */	
		if(selectedURL)
			safeURL = (NULL == strstr(selectedURL, "file://")) || isLocalDoc;
			
		if (!selectedURL) {
			gtk_menu_popup (GTK_MENU (make_html_menu ()), NULL, NULL,
				        NULL, NULL, event->button, event->time);
		} else {
			gdk_window_set_cursor (GDK_WINDOW (gtk_widget_get_parent_window (GTK_WIDGET (view))), NULL);
			gtk_menu_popup (GTK_MENU (make_url_menu (safeURL?selectedURL:"")), NULL, NULL,
				        NULL, NULL, event->button, event->time);
		}
		g_free (selectedURL);
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

	debug3(DEBUG_UPDATE, "action = '%s', method = '%s', encoding = '%s'", 
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
	HtmlDocument		*doc;	/**< HTML document the stream belongs to */
	HtmlStream		*stream;/**< currently processed stream */
	gpointer		view;	/**< HTML view the stream belongs to */
	struct updateJob	*job;	/**< currently processed HTTP request job (or NULL) */
} StreamData;

static void
gtkhtml2_view_connection_add (gpointer doc, StreamData *sd)
{
	GSList *connection_list;
		
	connection_list = g_object_get_data (G_OBJECT (doc), "connection_list");
	connection_list = g_slist_prepend (connection_list, sd);
	g_object_set_data (G_OBJECT (doc), "connection_list", connection_list);
}

static void
gtkhtml2_view_connection_remove (gpointer doc, StreamData *sd)
{
	GSList *connection_list;

	connection_list = g_object_get_data (G_OBJECT (doc), "connection_list");
	connection_list = g_slist_remove (connection_list, sd);
	g_object_set_data (G_OBJECT(doc), "connection_list", connection_list);
}

static void
gtkhtml2_view_free_stream_data (StreamData *sd)
{
	g_object_ref (sd->stream);
	html_stream_close (sd->stream);
	g_free (sd);
}

static void
gtkhtml2_view_stream_cancel (HtmlStream *stream, gpointer user_data, gpointer cancel_data)
{
	StreamData *sd = (StreamData *)cancel_data;
	
	debug1(DEBUG_UPDATE, "GtkHTML2: Canceling stream: %p", sd->stream);
	gtkhtml2_view_connection_remove (sd->doc, sd);
	update_job_cancel_by_owner (sd->stream);
	gtkhtml2_view_free_stream_data (sd);
}

static void
gtkhtml2_url_request_received_cb (const struct updateResult * const result, 
                                  gpointer user_data, 
				  updateFlags flags)
{
	StreamData *sd = (StreamData *)user_data;
	
	if (result->size != 0 && result->data) {
		html_stream_set_mime_type (sd->stream, result->contentType);
		html_stream_write (sd->stream, result->data, result->size); 
	}
	
	gtkhtml2_view_connection_remove (sd->doc, sd);
	gtkhtml2_view_free_stream_data (sd);
}

static void
gtkhtml2_view_request_url (HtmlDocument *doc, const gchar *url, HtmlStream *stream, gpointer view)
{
	gchar		*absURL, *base;
	gboolean	isLocalDoc;	
	
	g_assert (NULL != stream);

	base = g_object_get_data (G_OBJECT(doc), "liferea-base-uri");

	/* source document in local filesystem? */
	isLocalDoc = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (doc), "localDocument"));

	if (NULL != strstr(url, "file://") || isLocalDoc)
		absURL = g_strdup (url + strlen ("file://"));
	else
		absURL = common_build_url (url, base);

	if (absURL) {
		nodePtr displayedNode;
		updateRequestPtr request;
		
		StreamData *sd = g_malloc (sizeof (StreamData));
		sd->doc = doc;
		sd->view = view;
		sd->stream = stream;
		
		request = update_request_new();
		request->source = absURL;
	
		displayedNode = itemlist_get_displayed_node ();	
		if (displayedNode && displayedNode->subscription)
			request->options = update_options_copy (displayedNode->subscription->updateOptions);
		else
			request->options = g_new0 (struct updateOptions, 1);

		sd->job = update_execute_request (stream, request, gtkhtml2_url_request_received_cb, sd, FEED_REQ_PRIORITY_HIGH);
		gtkhtml2_view_connection_add (doc, sd);
		html_stream_set_cancel_func (stream, gtkhtml2_view_stream_cancel, sd);
	}
}

static void
on_url (HtmlView *view, const char *url, gpointer user_data)
{
	LifereaHtmlView *htmlview;
	xmlChar		*absURL;

	g_free (selectedURL);
	selectedURL = NULL;
	
	htmlview = g_object_get_data (G_OBJECT (user_data), "htmlview");

	if (url) {
		gdk_window_set_cursor (GDK_WINDOW (gtk_widget_get_parent_window (GTK_WIDGET (view))), link_cursor);
		absURL = common_build_url (url, g_object_get_data (G_OBJECT (HTML_VIEW (view)->document), "liferea-base-uri"));
		if (absURL) {
			selectedURL = g_strdup (absURL);
			liferea_htmlview_on_url (htmlview, selectedURL);
			g_free (absURL);
		}
	} else {
		gdk_window_set_cursor (GDK_WINDOW (gtk_widget_get_parent_window (GTK_WIDGET (view))), NULL);
		liferea_htmlview_on_url (htmlview, "");
	}
}

static gboolean request_object (HtmlView *view, GtkWidget *widget, gpointer user_data) {

	return FALSE;
}

static void
gtkhtml2_view_kill_old_connections (gpointer doc)
{
	GSList	*list, *iter;

	/* cancel requests caused by launch_url() */
	update_job_cancel_by_owner (doc);

	/* cancel indirectly caused requests */
	iter = list = g_object_get_data (G_OBJECT (doc), "connection_list");
	while (iter) {
		StreamData *sd = (StreamData *)iter->data;
		update_job_cancel_by_owner (sd->stream);
		gtkhtml2_view_free_stream_data (sd);
		iter = g_slist_next (iter);
	}
	g_slist_free (list);
	g_object_set_data (G_OBJECT (doc), "connection_list", NULL);
}

static void
link_clicked (HtmlDocument *doc, const gchar *url, gpointer view)
{
	xmlChar		*absURL;
	gboolean	safeURL, isLocalDoc;
	
	absURL = common_build_url (url, g_object_get_data (G_OBJECT (doc), "liferea-base-uri"));
	if (absURL) {
		/* source document in local filesystem? */
		isLocalDoc = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (doc), "localDocument"));

		/* prevent launching local filesystem links */	
		safeURL = (NULL == strstr (absURL, "file://")) || isLocalDoc;
		
		/* prevent local filesystem links */
		if (safeURL) {	
			LifereaHtmlView *htmlview = g_object_get_data (G_OBJECT (view), "htmlview");
			gtkhtml2_view_kill_old_connections (doc);
			liferea_htmlview_launch_URL (htmlview, absURL, GPOINTER_TO_INT (g_object_get_data (G_OBJECT (view), "internal_browsing")) ?  UI_HTMLVIEW_LAUNCH_INTERNAL: UI_HTMLVIEW_LAUNCH_DEFAULT);
		}
		xmlFree (absURL);
	}
}

void
gtkhtml2_destroyed_cb (GtkObject *scrollpane, gpointer user_data)
{
	HtmlDocument	*doc = HTML_VIEW (user_data)->document;
	
	gtkhtml2_view_kill_old_connections (doc);
}

static void
gtkhtml2_title_changed (HtmlDocument *doc, const gchar *new_title, gpointer data)
{
	ui_tabs_set_title (GTK_WIDGET(data), new_title);
}

/* ---------------------------------------------------------------------------- */
/* Liferea specific code to set up the HTML viewer widget 			*/
/* ---------------------------------------------------------------------------- */

/* adds a differences diff to the actual zoom level */
static void
gtkhtml2_change_zoom_level (GtkWidget *scrollpane, gfloat zoomLevel)
{
	GtkWidget *htmlwidget = gtk_bin_get_child (GTK_BIN (scrollpane));
	
	/* Clearing the selection is a workaround to avoid 
	   crashes when changing the zoomlevel as reported
	   in SF #1509741 */
	html_selection_clear (HTML_VIEW (htmlwidget));
	
	html_view_set_magnification (HTML_VIEW (htmlwidget), zoomLevel);
}

/* returns the currently set zoom level */
static gfloat
gtkhtml2_get_zoom_level (GtkWidget *scrollpane)
{
	GtkWidget *htmlwidget = gtk_bin_get_child (GTK_BIN (scrollpane));
	
	return html_view_get_magnification (HTML_VIEW (htmlwidget));
}

/* function to write HTML source given as a UTF-8 string. Note: Originally
   the same doc object was reused over and over. To avoid any problems 
   with this now a new one for each output is created... */
static void
gtkhtml2_write_html (GtkWidget *scrollpane,
                     const gchar *string,
                     guint length,
                     const gchar *base,
                     const gchar *contentType)
{
	gpointer	view = scrollpane;	// FIXME: have a real GObject type
	GtkWidget	*htmlwidget = gtk_bin_get_child (GTK_BIN (scrollpane));
	HtmlDocument	*doc = HTML_VIEW (htmlwidget)->document;

	/* finalizing older stuff */
	if (doc) {
		gtkhtml2_view_kill_old_connections (view);
		html_document_clear (doc);	/* heard rumors that this is necessary... */
		if (g_object_get_data (G_OBJECT (doc), "liferea-base-uri") != NULL)
			g_free (g_object_get_data (G_OBJECT (doc), "liferea-base-uri"));
		g_object_unref (G_OBJECT (doc));
	}
	
	doc = html_document_new ();
	html_view_set_document (HTML_VIEW (htmlwidget), doc);
	
	g_object_set_data (G_OBJECT (doc), "liferea-base-uri", g_strdup (base));
	g_object_set_data (G_OBJECT (doc), "localDocument", GINT_TO_POINTER (FALSE));
	
	html_document_clear (doc);
	/* Gtkhtml2 only responds to text/html documents, thus everything else must be converted to HTML in Liferea's code */
	html_document_open_stream (doc, "text/html");
	
	g_signal_connect (G_OBJECT (doc), "request_url",
				   GTK_SIGNAL_FUNC (gtkhtml2_view_request_url), view);
	
	g_signal_connect (G_OBJECT (doc), "submit",
				   GTK_SIGNAL_FUNC (on_submit), scrollpane);
	
	g_signal_connect (G_OBJECT (doc), "link_clicked",
				   G_CALLBACK (link_clicked), scrollpane);

	g_signal_connect (G_OBJECT (doc), "title_changed",
				   G_CALLBACK (gtkhtml2_title_changed), scrollpane);

	if (NULL == string || length == 0)
		html_document_write_stream (doc, EMPTY, strlen(EMPTY));	
	else if (contentType && !strcmp ("text/plain", contentType)) {
		gchar *tmp = g_markup_escape_text (string, length);
		html_document_write_stream (doc, "<html><head></head><body><pre>", strlen ("<html><head></head><body><pre>"));
		html_document_write_stream (doc, tmp, strlen (tmp));
		html_document_write_stream (doc, "</pre></body></html>", strlen ("</pre></body></html>"));
		g_free (tmp);
	} else {
		html_document_write_stream (doc, string, length);
	}
	
	html_document_close_stream (doc);

	gtkhtml2_change_zoom_level (view, gtkhtml2_get_zoom_level (view));	/* to enforce applying of changed zoom levels */
	gtkhtml2_scroll_to_top (view);
}

static GtkWidget *
gtkhtml2_new (LifereaHtmlView *htmlview, gboolean forceInternalBrowsing)
{
	gulong		handler;
	GtkWidget	*htmlwidget;
	GtkWidget	*scrollpane;
		
	link_cursor = gdk_cursor_new (GDK_HAND1);
	scrollpane = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollpane), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrollpane), GTK_SHADOW_IN);
	
	/* create html widget and pack it into the scrolled window */
	htmlwidget = html_view_new ();
	gtk_container_add (GTK_CONTAINER (scrollpane), GTK_WIDGET (htmlwidget));
	gtkhtml2_write_html (scrollpane, NULL, 0, "file:///", NULL);
	
	g_object_set_data (G_OBJECT (scrollpane), "htmlview", htmlview);
	g_object_set_data (G_OBJECT (scrollpane), "internal_browsing", GINT_TO_POINTER (forceInternalBrowsing));
	handler = g_signal_connect (G_OBJECT (htmlwidget), "on_url", G_CALLBACK (on_url), scrollpane);
		
	g_signal_connect (G_OBJECT (scrollpane), "destroy", G_CALLBACK (gtkhtml2_destroyed_cb), htmlwidget);
	g_signal_connect (G_OBJECT (htmlwidget), "button-press-event", G_CALLBACK (button_press_event), NULL);
	g_signal_connect (G_OBJECT (htmlwidget), "request_object", G_CALLBACK (request_object), NULL);
	
	gtk_widget_show (htmlwidget);

	return scrollpane;
}

static void gtkhtml2_init () { }

static void gtkhtml2_deinit () { }

static void
gtkhtml2_html_received (const struct updateResult * const result, gpointer user_data, updateFlags flags)
{
	gboolean	isLocalDoc;
	
	/* If no data was returned... */
	if (result->size == 0 || result->data == NULL) {
		/* Maybe an error message should be displayed.... */
		return; /* This should nicely exit.... */
	}
	ui_tabs_set_location (GTK_WIDGET (user_data), result->source);
	gtkhtml2_write_html (GTK_WIDGET (user_data), result->data, result->size,  result->source, result->contentType);
	
	/* determine if launched URL is a local one and set the flag to allow following local links */
	isLocalDoc = (result->source == strstr(result->source, "file://"));
	g_object_set_data (G_OBJECT (HTML_VIEW (gtk_bin_get_child (GTK_BIN (user_data)))->document),
	                   "localDocument", GINT_TO_POINTER (isLocalDoc));
}

static void
gtkhtml2_launch_url (GtkWidget *scrollpane, const gchar *url)
{
	updateRequestPtr	request;
	struct updateJob	*job;
	
	gtkhtml2_view_kill_old_connections (scrollpane);
	
	request = update_request_new ();
	request->options = g_new0 (struct updateOptions, 1);
	request->source = g_strdup (url);
	job = update_execute_request (scrollpane, request, gtkhtml2_html_received, scrollpane, FEED_REQ_PRIORITY_HIGH);
}

static gboolean
gtkhtml2_launch_inside_possible (void)
{
	return TRUE; 
}

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
	.api_version	= HTMLVIEW_PLUGIN_API_VERSION,
	.name		= "GtkHTML2",
	.priority	= 1,
	.externalCss	= FALSE,
	.plugin_init	= gtkhtml2_init,
	.plugin_deinit	= gtkhtml2_deinit,
	.create		= gtkhtml2_new,
	.write		= gtkhtml2_write_html,
	.launch		= gtkhtml2_launch_url,
	.launchInsidePossible = gtkhtml2_launch_inside_possible,
	.zoomLevelGet	= gtkhtml2_get_zoom_level,
	.zoomLevelSet	= gtkhtml2_change_zoom_level,
	.scrollPagedown	= gtkhtml2_scroll_pagedown,
	.setProxy	= NULL,
	.setOffLine	= NULL
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"GtkHTML2 Rendering Plugin",
	PLUGIN_TYPE_HTML_RENDERER,
	&gtkhtml2Info
};

DECLARE_PLUGIN(pi);
DECLARE_HTMLVIEW_PLUGIN(gtkhtml2Info);
