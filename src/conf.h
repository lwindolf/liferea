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

#define BROWSER_COMMAND		"/apps/liferea/browser"
#define GNOME_DEFAULT_BROWSER_COMMAND	"/desktop/gnome/url-handlers/unknown"
#define DEFAULT_BROWSER_COMMAND	"mozilla %s"
#define TIME_FORMAT		"/apps/liferea/timeformat"
	
#define PROXY_HOST		"/system/http_proxy/host"
#define PROXY_PORT		"/system/http_proxy/port"
#define USE_PROXY		"/system/http_proxy/use_http_proxy"

void	initConfig(void);
void	loadConfig(void);

/* feed/directory list entry manipulation methods */

void	loadEntries();

/* methods to modify folder contents */
GSList * getEntryKeyList(gchar *keyprefix);
void 	setEntryKeyList(gchar *keyprefix, GSList *newlist);

gchar *	addFolderToConfig(gchar *title);
void	removeFolderFromConfig(gchar *keyprefix);
gchar *	addEntryToConfig(gchar *keyprefix, gchar *url, gint type);
void	removeEntryFromConfig(gchar *keyprefix, gchar *feedkey);
gchar * getFreeEntryKey(gchar *keyprefix);

int	setEntryTitleInConfig(gchar *feedkey, gchar *feedname);
int	setEntryURLInConfig(gchar *feedkey, gchar *feedurl);
int	setFeedUpdateIntervalInConfig(gchar *feedkey, gint interval);
int	setFolderTitleInConfig(gchar *keyprefix, gchar *title);

/* returns true if namespace is enabled in configuration */
gboolean	getNameSpaceStatus(gchar *nsname);

/* used to enable/disable a namespace in configuration */
void		setNameSpaceStatus(gchar *nsname, gboolean enable);

/* preferences configuration methods */

gboolean 	getBooleanConfValue(gchar *valuename);
gchar *		getStringConfValue(gchar *valuename);
gint		getNumericConfValue(gchar  *valuename);

void 	setBooleanConfValue(gchar *valuename, gboolean value);
void	setStringConfValue(gchar *valuename, gchar *value);
void	setNumericConfValue(gchar  *valuename, gint value);

#endif
