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
#include "backend.h"

#include "rss_channel.h"
#include "rss_item.h"
#include "rss_ns.h"

#include "cdf_channel.h"
#include "cdf_item.h"

#include "ocs_dir.h"
#include "ocs_ns.h"

#include "conf.h"
#include "support.h"

#define BUFFER_SIZE 8192

#define HTML_WRITE(doc, tags)	{ if((NULL != tags) && (strlen(tags) > 0)) html_document_write_stream(doc, tags, strlen(tags)); }

/* common HTML definitions */

#define EMPTY		"<html><body>Item has no contents!</body></html>"
#define HTML_START	"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n<html>"
#define HTML_HEAD_START	"<head><title>itemview</title>"
#define META_ENCODING1	"<meta http-equiv=\"Content-Type\" content=\"text/html; charset="
#define META_ENCODING2	"\">"
#define HTML_HEAD_END	"</head><body>"

#define HTML_NEWLINE	"<br>"

#define HTML_END	"</body></html>"

/* RSS feed/item output definitions (some are used by OCS too!) */

#define ITEM_HEAD_START	"<table cellspacing=\"0\" style=\"margin-bottom:5px;width:100%;background:#D0D0D0;border-width:1px;border-style:solid;\"><tr><td style=\"padding:2px;padding-left:5px;padding-right:5px;\">"
#define ITEM_HEAD_CHANNEL	"<b>Feed: </b>"
#define ITEM_HEAD_ITEM		"<b>Item: </b>"
#define ITEM_HEAD_END	"</td></tr></table>"

#define FEED_HEAD_START		ITEM_HEAD_START
#define FEED_HEAD_CHANNEL	ITEM_HEAD_CHANNEL
#define FEED_HEAD_SOURCE	"<b>Source: </b>"
#define FEED_HEAD_END		ITEM_HEAD_END

#define FEED_FOOT_TABLE_START	"<table style=\"width:100%;border-width:1px;border-top-style:solid;border-color:#D0D0D0;\">"
#define FEED_FOOT_FIRSTTD	"<tr style=\"border-width:0;border-bottom-width:1px;border-style:dashed;border-color:#D0D0D0;\"><td><span style=\"font-size:8pt;color:#C0C0C0\">"
#define FEED_FOOT_NEXTTD	"</span></td><td><span style=\"font-size:8pt;color:#C0C0C0\">"
#define FEED_FOOT_LASTTD	"</span></td></tr>"
#define FEED_FOOT_TABLE_END	"</table>"

#define FEED_FOOT_WRITE(doc, key, value)	if(NULL != value) { \
							HTML_WRITE(doc, FEED_FOOT_FIRSTTD); \
							HTML_WRITE(doc, (gchar *)key); \
							HTML_WRITE(doc, FEED_FOOT_NEXTTD); \
							HTML_WRITE(doc, (gchar *)value); \
							HTML_WRITE(doc, FEED_FOOT_LASTTD); \
						}
						
#define	IMG_START	"<img style=\"margin-bottom:5px;\" src=\""
#define IMG_END		"\"><br>"

/* OCS direntry output definitions */

#define FORMAT_START	"<table cellspacing=\"0\" style=\"margin-bottom:5px;width:100%;background:#E0E0E0;border-color:#D0D0D0;border-width:1px;border-style:solid;\"><tr><td style=\"padding:2px\";>"
#define FORMAT_LINK	"<b>Format: </b>"
#define FORMAT_LANGUAGE		"</td></tr><tr><td style=\"padding:2px;border-color:#D0D0D0;border-width:0;border-top-width:1px;border-style:solid;\">Language: "
#define FORMAT_UPDATEPERIOD	"</td></tr><tr><td style=\"padding:2px;border-color:#D0D0D0;border-width:0;border-top-width:1px;border-style:solid;\">Update Period: "
#define FORMAT_UPDATEFREQUENCY	"</td></tr><tr><td style=\"padding:2px;border-color:#D0D0D0;border-width:0;border-top-width:1px;border-style:solid;\">Update Frequency: "
#define FORMAT_CONTENTTYPE	"</td></tr><tr><td style=\"padding:2px;border-color:#D0D0D0;border-width:0;border-top-width:1px;border-style:solid;\">Content Type: "
#define FORMAT_END	"</td></tr></table>"

/* declarations and globals for the gtkhtml callbacks */
typedef struct {
	HtmlDocument *doc;
	HtmlStream *stream;
	GnomeVFSAsyncHandle *handle;
} StreamData;

static HtmlDocument	*doc;

static GnomeVFSURI 	*baseURI = NULL;

/* structure for the hashtable callback which itself calls the 
   namespace output handler */
#define OUTPUT_CHANNEL_NS_HEADER	0
#define	OUTPUT_CHANNEL_NS_FOOTER	1
#define OUTPUT_ITEM_NS_HEADER		2
#define OUTPUT_ITEM_NS_FOOTER		3
typedef struct {
	gint		type;	
	gpointer	obj;	/* thats either a channelPtr or a itemPtr 
				   depending on the type value */
} outputRequest;

/* some prototypes */
static void url_requested(HtmlDocument *doc, const gchar *uri, HtmlStream *stream, gpointer data);
static void on_url (HtmlView *view, const char *url, gpointer user_data);
static void link_clicked (HtmlDocument *doc, const gchar *url, gpointer data);

/* creates and initializes the GtkHTML widget */
void setupHTMLView(GtkWidget *mainwindow) {
	GtkWidget	*scrolledwindow;
	GtkWidget	*pane;
	GtkWidget	*htmlwidget;	
	char testhtml[] = "<html><body></body></html>";	// FIXME
	
	/* prepare HTML widget */
	doc = html_document_new();
	html_document_open_stream(doc, "text/html");
	html_document_write_stream(doc, testhtml, strlen (testhtml));
	html_document_close_stream(doc);
	
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
	
	g_signal_connect(G_OBJECT (doc), "request_url",
			 GTK_SIGNAL_FUNC (url_requested), htmlwidget);	
			 
	g_signal_connect (G_OBJECT (htmlwidget), "on_url",
			  G_CALLBACK (on_url), lookup_widget(mainwindow, "statusbar"));

	g_signal_connect (G_OBJECT (doc), "link_clicked",
			  G_CALLBACK (link_clicked), NULL);

	gtk_widget_show_all(scrolledwindow);
}

/* method called by g_hash_table_foreach from inside the HTML
   generator functions to output namespace specific infos */
void showFeedNSInfo(gpointer key, gpointer value, gpointer userdata) {
	outputRequest	*request = (outputRequest *)userdata;
	RSSNsHandler	*nsh = (RSSNsHandler *)value;
	outputFunc	fp;

	switch(request->type) {
		case OUTPUT_CHANNEL_NS_HEADER:
			fp = nsh->doChannelHeaderOutput;
			if(NULL != fp)
				(*fp)(request->obj, doc);
			break;
		case OUTPUT_CHANNEL_NS_FOOTER:
			fp = nsh->doChannelFooterOutput;
			if(NULL != fp)
				(*fp)(request->obj, doc);
			break;
		case OUTPUT_ITEM_NS_HEADER:
			fp = nsh->doItemHeaderOutput;
			if(NULL != fp)
				(*fp)(request->obj, doc);
			break;		
		case OUTPUT_ITEM_NS_FOOTER:
			fp = nsh->doItemFooterOutput;
			if(NULL != fp)
				(*fp)(request->obj, doc);
			break;			
		default:	
			g_warning(_("Internal error! Invalid output request mode for namespace information!"));
			break;		
	}
}

/* writes item description as HTML into the gtkhtml widget */
void showItem(gpointer ip, gpointer cp) {
	GHashTable	*RSSNsHandler;
	gchar		*itemlink;
	gchar		*feedimage;
	gchar		*tmp;	
	outputRequest	request;
	
	g_assert(doc != NULL);
	g_assert(cp != NULL);
	g_assert(ip != NULL);
	html_document_open_stream(doc, "text/html");
	HTML_WRITE(doc, HTML_START);
	HTML_WRITE(doc, HTML_HEAD_START);

	HTML_WRITE(doc, META_ENCODING1);
	HTML_WRITE(doc, "UTF-8");
	HTML_WRITE(doc, META_ENCODING2);

	HTML_WRITE(doc, HTML_HEAD_END);

	if(NULL != (itemlink = getRSSItemTag(ip, ITEM_LINK))) {
		HTML_WRITE(doc, ITEM_HEAD_START);
		
		HTML_WRITE(doc, ITEM_HEAD_CHANNEL);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", getFeedTag(cp, CHANNEL_LINK), getDefaultEntryTitle(((channelPtr)cp)->key));
		HTML_WRITE(doc, tmp);
		g_free(tmp);
		
		HTML_WRITE(doc, HTML_NEWLINE);
		
		HTML_WRITE(doc, ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", itemlink, getRSSItemTag(ip, ITEM_TITLE));
		HTML_WRITE(doc, tmp);
		g_free(tmp);
		
		HTML_WRITE(doc, ITEM_HEAD_END);	
	}	

	/* process namespace infos */
	RSSNsHandler = getFeedNsHandler(cp);
	request.obj = (gpointer)ip;
	request.type = OUTPUT_ITEM_NS_HEADER;	
	if(NULL != RSSNsHandler)
		g_hash_table_foreach(RSSNsHandler, showFeedNSInfo, (gpointer)&request);

	if(NULL != (feedimage = getFeedTag(cp, CHANNEL_IMAGE))) {
		HTML_WRITE(doc, IMG_START);
		HTML_WRITE(doc, feedimage);
		HTML_WRITE(doc, IMG_END);	
	}

	if(NULL != getRSSItemTag(ip, ITEM_DESCRIPTION))
		HTML_WRITE(doc, getRSSItemTag(ip, ITEM_DESCRIPTION));

	request.type = OUTPUT_ITEM_NS_FOOTER;
	if(NULL != RSSNsHandler)
		g_hash_table_foreach(RSSNsHandler, showFeedNSInfo, (gpointer)&request);


	HTML_WRITE(doc, HTML_END);
	html_document_close_stream(doc);
}

/* writes RSS channel description as HTML into the gtkhtml widget */
void showFeedInfo(gpointer cp) {
	GHashTable	*RSSNsHandler;
	gchar		*feedimage;
	gchar		*feeddescription;
	gchar		*tmp;	
	outputRequest	request;

	g_assert(doc != NULL);
	g_assert(cp != NULL);	
	html_document_open_stream(doc, "text/html");
	HTML_WRITE(doc, HTML_START);
	HTML_WRITE(doc, HTML_HEAD_START);

	HTML_WRITE(doc, META_ENCODING1);
	HTML_WRITE(doc, "UTF-8");
	HTML_WRITE(doc, META_ENCODING2);

	HTML_WRITE(doc, HTML_HEAD_END);

	HTML_WRITE(doc, FEED_HEAD_START);
	
	HTML_WRITE(doc, FEED_HEAD_CHANNEL);
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>", getFeedTag(cp, CHANNEL_LINK), getDefaultEntryTitle(((channelPtr)cp)->key));
	HTML_WRITE(doc, tmp);
	g_free(tmp);
	
	HTML_WRITE(doc, HTML_NEWLINE);	

	HTML_WRITE(doc, FEED_HEAD_SOURCE);	
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>", getEntrySource(((channelPtr)cp)->key), getEntrySource(((channelPtr)cp)->key));
	HTML_WRITE(doc, tmp);
	g_free(tmp);

	HTML_WRITE(doc, FEED_HEAD_END);	
		
	/* process namespace infos */
	RSSNsHandler = getFeedNsHandler(cp);
	request.obj = (gpointer)cp;
	request.type = OUTPUT_CHANNEL_NS_HEADER;	
	if(NULL != RSSNsHandler)
		g_hash_table_foreach(RSSNsHandler, showFeedNSInfo, (gpointer)&request);

	if(NULL != (feedimage = getFeedTag(cp, CHANNEL_IMAGE))) {
		HTML_WRITE(doc, IMG_START);
		HTML_WRITE(doc, feedimage);
		HTML_WRITE(doc, IMG_END);	
	}

	if(NULL != (feeddescription = getFeedTag(cp, CHANNEL_DESCRIPTION)))
		HTML_WRITE(doc, feeddescription);

	HTML_WRITE(doc, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(doc, "language",		getFeedTag(cp, CHANNEL_LANGUAGE));
	FEED_FOOT_WRITE(doc, "copyright",		getFeedTag(cp, CHANNEL_COPYRIGHT));
	FEED_FOOT_WRITE(doc, "last build date",		getFeedTag(cp, CHANNEL_LASTBUILDDATE));
	FEED_FOOT_WRITE(doc, "publication date",	getFeedTag(cp, CHANNEL_PUBDATE));
	FEED_FOOT_WRITE(doc, "webmaster",		getFeedTag(cp, CHANNEL_WEBMASTER));
	FEED_FOOT_WRITE(doc, "managing editor",		getFeedTag(cp, CHANNEL_MANAGINGEDITOR));
	FEED_FOOT_WRITE(doc, "category",		getFeedTag(cp, CHANNEL_CATEGORY));
	HTML_WRITE(doc, FEED_FOOT_TABLE_END);
	
	/* process namespace infos */
	request.type = OUTPUT_CHANNEL_NS_FOOTER;
	if(NULL != RSSNsHandler)
		g_hash_table_foreach(RSSNsHandler, showFeedNSInfo, (gpointer)&request);

	HTML_WRITE(doc, HTML_END);
	html_document_close_stream(doc);
}


/* writes CDF item description as HTML into the gtkhtml widget */
void showCDFItem(gpointer ip, gpointer cp) {
	gchar		*itemlink;
	gchar		*feedimage;
	gchar		*tmp;	
	
	g_assert(doc != NULL);
	g_assert(cp != NULL);
	g_assert(ip != NULL);
	html_document_open_stream(doc, "text/html");
	HTML_WRITE(doc, HTML_START);
	HTML_WRITE(doc, HTML_HEAD_START);

	HTML_WRITE(doc, META_ENCODING1);
	HTML_WRITE(doc, "UTF-8");
	HTML_WRITE(doc, META_ENCODING2);

	HTML_WRITE(doc, HTML_HEAD_END);

	if(NULL != (itemlink = getCDFItemTag(ip, CDF_ITEM_LINK))) {
		HTML_WRITE(doc, ITEM_HEAD_START);
		
		HTML_WRITE(doc, ITEM_HEAD_CHANNEL);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", getEntrySource(((CDFChannelPtr)cp)->key), getDefaultEntryTitle(((CDFChannelPtr)cp)->key));
		HTML_WRITE(doc, tmp);
		g_free(tmp);
		
		HTML_WRITE(doc, HTML_NEWLINE);
		
		HTML_WRITE(doc, ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", itemlink, getCDFItemTag(ip, CDF_ITEM_TITLE));
		HTML_WRITE(doc, tmp);
		g_free(tmp);
		
		HTML_WRITE(doc, ITEM_HEAD_END);	
	}	

	if(NULL != (feedimage = getCDFItemTag(ip, CDF_ITEM_IMAGE))) {
		HTML_WRITE(doc, IMG_START);
		HTML_WRITE(doc, feedimage);
		HTML_WRITE(doc, IMG_END);	
	}

	if(NULL != getCDFItemTag(ip, CDF_ITEM_DESCRIPTION))
		HTML_WRITE(doc, getCDFItemTag(ip, CDF_ITEM_DESCRIPTION));

	HTML_WRITE(doc, HTML_END);
	html_document_close_stream(doc);
}

/* writes CDF channel description as HTML into the gtkhtml widget */
void showCDFFeedInfo(gpointer cp) {
	gchar		*feedimage;
	gchar		*feeddescription;
	gchar		*source;
	gchar		*tmp;

	g_assert(doc != NULL);
	g_assert(cp != NULL);	
	html_document_open_stream(doc, "text/html");
	HTML_WRITE(doc, HTML_START);
	HTML_WRITE(doc, HTML_HEAD_START);

	HTML_WRITE(doc, META_ENCODING1);
	HTML_WRITE(doc, "UTF-8");
	HTML_WRITE(doc, META_ENCODING2);

	HTML_WRITE(doc, HTML_HEAD_END);

	HTML_WRITE(doc, FEED_HEAD_START);
	
	HTML_WRITE(doc, FEED_HEAD_CHANNEL);
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>", getEntrySource(((CDFChannelPtr)cp)->key), getDefaultEntryTitle(((CDFChannelPtr)cp)->key));
	HTML_WRITE(doc, tmp);
	g_free(tmp);
	
	HTML_WRITE(doc, FEED_HEAD_END);	

	if(NULL != (feedimage = getCDFFeedTag(cp, CDF_CHANNEL_IMAGE))) {
		HTML_WRITE(doc, IMG_START);
		HTML_WRITE(doc, feedimage);
		HTML_WRITE(doc, IMG_END);	
	}

	if(NULL != (feeddescription = getCDFFeedTag(cp, CDF_CHANNEL_DESCRIPTION)))
		HTML_WRITE(doc, feeddescription);

	HTML_WRITE(doc, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(doc, "copyright",		getCDFFeedTag(cp, CDF_CHANNEL_COPYRIGHT));
	FEED_FOOT_WRITE(doc, "publication date",	getCDFFeedTag(cp, CDF_CHANNEL_PUBDATE));
	FEED_FOOT_WRITE(doc, "webmaster",		getCDFFeedTag(cp, CDF_CHANNEL_WEBMASTER));
	FEED_FOOT_WRITE(doc, "category",		getCDFFeedTag(cp, CDF_CHANNEL_CATEGORY));
	HTML_WRITE(doc, FEED_FOOT_TABLE_END);

	HTML_WRITE(doc, HTML_END);
	html_document_close_stream(doc);
}

/* print information of a format entry in the HTML */
static void showFormatEntry(gpointer data, gpointer userdata) {
	gchar		*link, *tmp;
	
	if(NULL != (link = getOCSFormatSource(data))) {
		HTML_WRITE(doc, FORMAT_START);

		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", link, link);
		HTML_WRITE(doc, FORMAT_LINK);
		HTML_WRITE(doc, tmp);		
		g_free(tmp);

		if(NULL != (tmp = getOCSFormatTag(data, OCS_LANGUAGE))) {
			HTML_WRITE(doc, FORMAT_LANGUAGE);
			HTML_WRITE(doc, tmp);
		}

		if(NULL != (tmp = getOCSFormatTag(data, OCS_UPDATEPERIOD))) {
			HTML_WRITE(doc, FORMAT_UPDATEPERIOD);
			HTML_WRITE(doc, tmp);
		}

		if(NULL != (tmp = getOCSFormatTag(data, OCS_UPDATEFREQUENCY))) {
			HTML_WRITE(doc, FORMAT_UPDATEFREQUENCY);
			HTML_WRITE(doc, tmp);
		}
		
		if(NULL != (tmp = getOCSFormatTag(data, OCS_CONTENTTYPE))) {
			HTML_WRITE(doc, FORMAT_CONTENTTYPE);
			HTML_WRITE(doc, tmp);
		}
		
		HTML_WRITE(doc, FORMAT_END);	
	}
}

/* display a directory entry description and its formats in the HTML widget */
void showDirectoryEntry(gpointer dep, gpointer dp) {
	GSList		*iter;
	gchar		*link;
	gchar		*channelimage;
	gchar		*tmp;
	gpointer	fp;
	
	g_assert(doc != NULL);
	g_assert(dep != NULL);
	g_assert(dp != NULL);
	html_document_open_stream(doc, "text/html");
	HTML_WRITE(doc, HTML_START);
	HTML_WRITE(doc, HTML_HEAD_START);

	HTML_WRITE(doc, META_ENCODING1);
	HTML_WRITE(doc, "UTF-8");
	HTML_WRITE(doc, META_ENCODING2);

	HTML_WRITE(doc, HTML_HEAD_END);

	if(NULL != (link = getOCSDirEntrySource(dep))) {
		HTML_WRITE(doc, ITEM_HEAD_START);
	
		HTML_WRITE(doc, ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", link, getOCSDirEntryTag(dep, OCS_TITLE));
		HTML_WRITE(doc, tmp);
		g_free(tmp);
		
		HTML_WRITE(doc, ITEM_HEAD_END);	
	}

	if(NULL != (channelimage = getOCSDirEntryTag(dep, OCS_IMAGE))) {
		HTML_WRITE(doc, IMG_START);
		HTML_WRITE(doc, channelimage);
		HTML_WRITE(doc, IMG_END);	
	}

	if(NULL != getOCSDirEntryTag(dep, OCS_DESCRIPTION))
		HTML_WRITE(doc, getOCSDirEntryTag(dep, OCS_DESCRIPTION));
		
	/* output infos about the available formats */
	g_slist_foreach(((dirEntryPtr)dep)->formats, showFormatEntry, NULL);

	HTML_WRITE(doc, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(doc, "creator",		getOCSDirEntryTag(dep, OCS_CREATOR));
	FEED_FOOT_WRITE(doc, "subject",		getOCSDirEntryTag(dep, OCS_SUBJECT));
	FEED_FOOT_WRITE(doc, "language",	getOCSDirEntryTag(dep, OCS_LANGUAGE));
	FEED_FOOT_WRITE(doc, "updatePeriod",	getOCSDirEntryTag(dep, OCS_UPDATEPERIOD));
	FEED_FOOT_WRITE(doc, "contentType",	getOCSDirEntryTag(dep, OCS_CONTENTTYPE));
	HTML_WRITE(doc, FEED_FOOT_TABLE_END);

	HTML_WRITE(doc, HTML_END);
	html_document_close_stream(doc);
}

/* writes directory info as HTML */
void showDirectoryInfo(gpointer dp) {
	gchar		*description;
	gchar		*source;
	gchar		*tmp;	

	g_assert(doc != NULL);
	g_assert(dp != NULL);	
	html_document_open_stream(doc, "text/html");
	HTML_WRITE(doc, HTML_START);
	HTML_WRITE(doc, HTML_HEAD_START);

	HTML_WRITE(doc, META_ENCODING1);
	HTML_WRITE(doc, "UTF-8");
	HTML_WRITE(doc, META_ENCODING2);

	HTML_WRITE(doc, HTML_HEAD_END);

	HTML_WRITE(doc, FEED_HEAD_START);
	
	HTML_WRITE(doc, FEED_HEAD_CHANNEL);
	HTML_WRITE(doc, getDefaultEntryTitle(((directoryPtr)dp)->key));

	HTML_WRITE(doc, HTML_NEWLINE);	
// FIXME: segfaults with Moreover....
/*
	HTML_WRITE(doc, FEED_HEAD_SOURCE);	
	if(NULL != (source = getOCSDirectorySource(((directoryPtr)dp)->key))) {
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", source, source);
		HTML_WRITE(doc, tmp);
		g_free(tmp);
	}

*/	HTML_WRITE(doc, FEED_HEAD_END);	

	if(NULL != (description = getOCSDirectoryTag(dp, OCS_DESCRIPTION)))
		HTML_WRITE(doc, description);

	HTML_WRITE(doc, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(doc, "creator",		getOCSDirectoryTag(dp, OCS_CREATOR));
	FEED_FOOT_WRITE(doc, "subject",		getOCSDirectoryTag(dp, OCS_SUBJECT));
	FEED_FOOT_WRITE(doc, "language",	getOCSDirectoryTag(dp, OCS_LANGUAGE));
	FEED_FOOT_WRITE(doc, "updatePeriod",	getOCSDirectoryTag(dp, OCS_UPDATEPERIOD));
	FEED_FOOT_WRITE(doc, "contentType",	getOCSDirectoryTag(dp, OCS_CONTENTTYPE));
	HTML_WRITE(doc, FEED_FOOT_TABLE_END);

	HTML_WRITE(doc, HTML_END);
	html_document_close_stream(doc);
}


/* ------------------------------------------------------------------------------- */
/* GtkHTML Callbacks taken from browser-window.c of libgtkhtml-2.2.0 
   these are needed to automatically resolve links in the HTML
   written by showItem() */ 

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

static void
link_clicked (HtmlDocument *doc, const gchar *url, gpointer data)
{
	gchar	*cmd;
	gchar	*statusline;

	// BROWSER_COMMAND should better contain a %s, checking this?
	cmd = g_strdup_printf(getStringConfValue(BROWSER_COMMAND), url);	
	cmd = g_strdup_printf("%s &", cmd);	/* to lazy to fork+exec... */
	if(-1 == system(cmd))
		statusline = g_strdup_printf("browser command failed: %s", g_strerror(errno));
	else	
		statusline = g_strdup_printf("starting: \"%s\"", cmd);
		
	print_status(statusline);
	g_free(statusline);	
}
