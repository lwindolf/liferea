/*
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

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

#ifndef _HTMLVIEW_H
#define _HTMLVIEW_H

#include <glib.h>
#include <libgtkhtml/gtkhtml.h>
#include <libgnomevfs/gnome-vfs.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

/* Common HTML definitions, style should be defined in the
   liferea.css style sheet! For some important HTML literals
   like the item and feed description headers the styles 
   are duplicated here just in case the style sheet is 
   missing. */

#define EMPTY		"<html><body></body></html>"
#define HTML_START	"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n<html>"
#define HTML_HEAD_START	"<head><title>item view</title>"
#define META_ENCODING1	"<meta http-equiv=\"Content-Type\" content=\"text/html; charset="
#define META_ENCODING2	"\">"
#define HTML_HEAD_END	"</head><body>"
#define HTML_HEAD_END2	"</head><body style=\"padding:0px;\">"

#define HTML_NEWLINE	"<br>"

#define HTML_END	"</body></html>"

/* RSS feed/item output definitions (some are used by OCS too!) */

#define ITEM_HEAD_START		"<table cellspacing=\"0\" class=\"itemhead\"><tr><td class=\"itemhead\">"
#define ITEM_HEAD_CHANNEL	"<b>Feed: </b>"
#define ITEM_HEAD_ITEM		"<b>Item: </b>"
#define ITEM_HEAD_END		"</td></tr></table>"

#define FEED_HEAD_START		ITEM_HEAD_START
#define FEED_HEAD_CHANNEL	ITEM_HEAD_CHANNEL
#define FEED_HEAD_SOURCE	"<b>Source: </b>"
#define FEED_HEAD_END		ITEM_HEAD_END

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
#define HTTP_ERROR_TEXT		"The last update of this subscription failed!<br><b>HTTP error code %d: %s</b>"
#define PARSE_ERROR_TEXT	"There were errors while parsing this feed. The following error occured:<br><b>%s</b>"
#define UPDATE_ERROR_END	"</td></tr></table>"

/* creates the HTML widget */
void	setupHTMLViews(GtkWidget *mainwindow, GtkWidget *pane1, GtkWidget *pane2, gint initialZoomLevel);

/* loads a emtpy HTML page */
void clearHTMLView(void);

/* function to select either the single item view (3 pane mode)
   or the item list view (2 pane mode) */
void	setHTMLViewMode(gboolean threePane);

/* functions to output HTML to the selected HTML widget */
void	startHTML(gchar **buffer, gboolean padded);

void	writeHTML(gchar *string);

void	finishHTML(gchar **buffer);

void	writeStyleSheetLink(gchar **buffer, gchar *styleSheetFile);
void	writeStyleSheetLinks(gchar **buffer);

/* to launch any URL */
void 	launchURL(const gchar *url);

/* launches the specified URL */
void launchURL(const gchar *url);

/* adds a differences diff to the actual zoom level */
void changeZoomLevel(gfloat diff);

/* returns the currently set zoom level */
gfloat getZoomLevel(void);

#endif
