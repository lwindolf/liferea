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

#ifndef _CONF_H
#define _CONF_H

#include <glib.h>

/* these defines don't belong here! */
#define BROWSER_COMMAND		"/apps/liferea/browser"
#define GNOME_DEFAULT_BROWSER_COMMAND	"/desktop/gnome/url-handlers/unknown"
#define DEFAULT_BROWSER_COMMAND	"mozilla %s"
#define TIME_FORMAT		"/apps/liferea/timeformat"
#define USE_DC			"/apps/liferea/ns_dc"
#define USE_FM			"/apps/liferea/ns_fm"
#define USE_CONTENT		"/apps/liferea/ns_content"
#define USE_SLASH		"/apps/liferea/ns_slash"
	
#define PROXY_HOST		"/system/http_proxy/host"
#define PROXY_PORT		"/system/http_proxy/port"
#define USE_PROXY		"/system/http_proxy/use_http_proxy"

void	initConfig(void);
void	loadConfig(void);

/* feed/directory list entry manipulation methods */

void	loadEntries();
GSList * getEntryKeyList(gchar *keyprefix);

gchar *	addFolderToConfig(gchar *title);
void	removeFolderFromConfig(gchar *keyprefix);
gchar *	addEntryToConfig(gchar *keyprefix, gchar *url, gint type);
void	removeEntryFromConfig(gchar *keyprefix, gchar *feedkey);
gchar * getFreeEntryKey(gchar *keyprefix);

// FIXME: setEntryTypeInConfig(gchar *feedkey, gint type);
int	setEntryTitleInConfig(gchar *feedkey, gchar *feedname);
int	setEntryURLInConfig(gchar *feedkey, gchar *feedurl);
int	setFeedUpdateIntervalInConfig(gchar *feedkey, gint interval);

void	moveUpEntryPositionInConfig(gchar *keyprefix, gchar *key);
void	moveDownEntryPositionInConfig(gchar *keyprefix, gchar *key);
void	sortEntryKeyList(gchar *keyprefix);

/* preferences configuration methods */

gboolean 	getBooleanConfValue(gchar *valuename);
gchar *		getStringConfValue(gchar *valuename);
gint		getNumericConfValue(gchar  *valuename);

void 	setBooleanConfValue(gchar *valuename, gboolean value);
void	setStringConfValue(gchar *valuename, gchar *value);
void	setNumericConfValue(gchar  *valuename, gint value);

#endif
