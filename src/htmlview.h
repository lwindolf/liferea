/*
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

#ifndef _HTMLVIEW_H
#define _HTMLVIEW_H

#include <glib.h>
#include <libgtkhtml/gtkhtml.h>
#include <libgnomevfs/gnome-vfs.h>


// FIXME: move not common defines to specific feed handler module

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

#define ITEM_HEAD_START	"<table cellspacing=\"0\" style=\"margin-bottom:5px;width:100%;background:#D5C5B5;border-width:1px;border-style:solid;\"><tr><td style=\"padding:2px;padding-left:5px;padding-right:5px;\">"
#define ITEM_HEAD_CHANNEL	"<b>Feed: </b>"
#define ITEM_HEAD_ITEM		"<b>Item: </b>"
#define ITEM_HEAD_END	"</td></tr></table>"

#define FEED_HEAD_START		ITEM_HEAD_START
#define FEED_HEAD_CHANNEL	ITEM_HEAD_CHANNEL
#define FEED_HEAD_SOURCE	"<b>Source: </b>"
#define FEED_HEAD_END		ITEM_HEAD_END

#define FEED_FOOT_TABLE_START	"<table style=\"width:100%;border-width:1px;border-top-style:solid;border-color:#D0D0D0;\">"
#define FEED_FOOT_FIRSTTD	"<tr style=\"border-width:0;border-bottom-width:1px;border-style:dashed;border-color:#D0D0D0;\"><td width=\"30%\"><span style=\"font-size:8pt;color:#C0C0C0\">"
#define FEED_FOOT_NEXTTD	"</span></td><td><span style=\"font-size:8pt;color:#C0C0C0\">"
#define FEED_FOOT_LASTTD	"</span></td></tr>"
#define FEED_FOOT_TABLE_END	"</table>"

#define FEED_FOOT_WRITE(doc, key, value)	if(NULL != value) { \
							writeHTML(FEED_FOOT_FIRSTTD); \
							writeHTML((gchar *)key); \
							writeHTML(FEED_FOOT_NEXTTD); \
							writeHTML((gchar *)value); \
							writeHTML(FEED_FOOT_LASTTD); \
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


/* creates the HTML widget */
void 	setupHTMLView(GtkWidget *mainwindow);

void	startHTMLOutput(void);

void	writeHTML(gchar *string);

void	finishHTMLOutput(void);

/* to launch any URL */
void 	launchURL(const gchar *url);
#endif
