/**
 * @file conf.h Liferea configuration (gconf access and feedlist import)
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _CONF_H
#define _CONF_H

#include <glib.h>
#include "folder.h"

/* browsing settings */
#define GNOME_BROWSER_ENABLED		"/apps/liferea/browsermode"
#define BROWSER_COMMAND			"/apps/liferea/browser"
#define BROWSER_MODULE			"/apps/liferea/browser-module"
#define GNOME_DEFAULT_BROWSER_COMMAND	"gnome-open '%s'"
#define DEFAULT_BROWSER_COMMAND		"mozilla '%s'"
#define DEFAULT_FONT			"/desktop/gnome/interface/font_name"
#define USER_FONT			"/apps/liferea/browser-font"
#define BROWSE_INSIDE_APPLICATION	"/apps/liferea/browse-inside-application"

/* item list settings */
#define TIME_FORMAT			"/apps/liferea/timeformat"
#define TIME_FORMAT_MODE		"/apps/liferea/timeformatmode"

/* feed handling settings */
#define DEFAULT_MAX_ITEMS		"/apps/liferea/maxitemcount"
#define UPDATE_ON_STARTUP		"/apps/liferea/updateonstartup"
#define DISABLE_HELPFEEDS		"/apps/liferea/disable-helpfeeds"
#define UPDATE_THREAD_CONCURRENCY	"/apps/liferea/update-thread-concurrency"

/* GUI settings and persistency values */
#define SHOW_TRAY_ICON			"/apps/liferea/trayicon"
#define DISABLE_MENUBAR			"/apps/liferea/disable-menubar"
#define DISABLE_TOOLBAR			"/apps/liferea/disable-toolbar"
#define LAST_WINDOW_X			"/apps/liferea/last-window-x"
#define LAST_WINDOW_Y			"/apps/liferea/last-window-y"
#define LAST_WINDOW_WIDTH		"/apps/liferea/last-window-width"
#define LAST_WINDOW_HEIGHT		"/apps/liferea/last-window-height"
#define LAST_VPANE_POS			"/apps/liferea/last-vpane-pos"
#define LAST_HPANE_POS			"/apps/liferea/last-hpane-pos"
#define LAST_ITEMLIST_MODE		"/apps/liferea/last-itemlist-mode"
#define LAST_ZOOMLEVEL			"/apps/liferea/last-zoomlevel"

/* networking settings */
#define PROXY_HOST			"/system/http_proxy/host"
#define PROXY_PORT			"/system/http_proxy/port"
#define USE_PROXY			"/system/http_proxy/use_http_proxy"

/* initializing methods */
void	initConfig(void);
void	loadConfig(void);

/* config loading on startup */
void	loadEntries(void);

/* feed/directory list entry manipulation methods */
gchar* conf_new_id();
void	loadSubscriptions();
void conf_feedlist_schedule_save();
void conf_feedlist_save();

/* methods to modify folder contents */
GSList * getFeedKeyList(gchar *keyprefix);
void 	setFeedKeyList(gchar *keyprefix, GSList *newlist);

folderPtr feedlist_insert_help_folder(folderPtr parent);

gchar * getFreeFeedKey(gchar *keyprefix);

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
