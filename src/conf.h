/**
 * @file conf.h Liferea configuration (gconf access)
 *
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004,2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#define BROWSE_INSIDE_APPLICATION	"/apps/liferea/browse-inside-application"
#define BROWSE_KEY_SETTING		"/apps/liferea/browse-key-setting"
#define BROWSER_ID			"/apps/liferea/browser_id"
#define BROWSER_PLACE			"/apps/liferea/browser_place"
#define BROWSER_COMMAND			"/apps/liferea/browser"
#define BROWSER_MODULE			"/apps/liferea/browser-module"
#define GNOME_DEFAULT_BROWSER_COMMAND	"gnome-open '%s'"
#define DEFAULT_BROWSER_COMMAND		"mozilla '%s'"
#define DEFAULT_FONT			"/desktop/gnome/interface/font_name"
#define USER_FONT			"/apps/liferea/browser-font"
#define REFOCUS_TIMEOUT			"/apps/liferea/refocus-timeout"
#define DISABLE_JAVASCRIPT		"/apps/liferea/disable-javascript"

/* enclosure handling */
#define ENCLOSURE_DOWNLOAD_TOOL		"/apps/liferea/enclosure-download-tool"

/* item list settings */
#define TIME_FORMAT			"/apps/liferea/timeformat"
#define TIME_FORMAT_MODE		"/apps/liferea/timeformatmode"

/* feed handling settings */
#define DEFAULT_MAX_ITEMS		"/apps/liferea/maxitemcount"
#define DEFAULT_UPDATE_INTERVAL		"/apps/liferea/default-update-interval"
#define STARTUP_FEED_ACTION		"/apps/liferea/startup_feed_action"
#define UPDATE_THREAD_CONCURRENCY	"/apps/liferea/update-thread-concurrency"
#define KEEP_FEEDS_IN_MEMORY		"/apps/liferea/keep-feeds-in-memory"
#define DISABLE_SUBSCRIPTION_PIPE	"/apps/liferea/disable-subscription-pipe"

/* folder handling settings */
#define FOLDER_DISPLAY_MODE		"/apps/liferea/folder-display-mode"

/* GUI settings and persistency values */
#define SHOW_TRAY_ICON			"/apps/liferea/trayicon"
#define SHOW_POPUP_WINDOWS		"/apps/liferea/show-popup-windows"
#define POPUP_PLACEMENT			"/apps/liferea/popup-placement"
#define DISABLE_MENUBAR			"/apps/liferea/disable-menubar"
#define DISABLE_TOOLBAR			"/apps/liferea/disable-toolbar"
#define LAST_WINDOW_X			"/apps/liferea/last-window-x"
#define LAST_WINDOW_Y			"/apps/liferea/last-window-y"
#define LAST_WINDOW_WIDTH		"/apps/liferea/last-window-width"
#define LAST_WINDOW_HEIGHT		"/apps/liferea/last-window-height"
#define LAST_WINDOW_MAXIMIZED		"/apps/liferea/last-window-maximized"
#define LAST_VPANE_POS			"/apps/liferea/last-vpane-pos"
#define LAST_HPANE_POS			"/apps/liferea/last-hpane-pos"
#define LAST_ZOOMLEVEL			"/apps/liferea/last-zoomlevel"

/* networking settings */
#define USE_PROXY			"/system/http_proxy/use_http_proxy"
#define PROXY_HOST			"/system/http_proxy/host"
#define PROXY_PORT			"/system/http_proxy/port"
#define PROXY_USEAUTH			"/system/http_proxy/use_authentication"
#define PROXY_USER			"/system/http_proxy/authentication_user"
#define PROXY_PASSWD			"/system/http_proxy/authentication_password"

/* initializing methods */
void	conf_init(void);
void	conf_load(void);

/* feed/directory list entry manipulation methods */
gchar*	conf_new_id();
void	conf_load_subscriptions();
void 	conf_feedlist_schedule_save();
void 	conf_feedlist_save();

/* methods to modify folder contents */
GSList * getFeedKeyList(gchar *keyprefix);
void 	setFeedKeyList(gchar *keyprefix, GSList *newlist);

folderPtr feedlist_insert_help_folder(folderPtr parent, GTimeVal *lastPoll, GTimeVal *lastFaviconPoll);

gchar * getFreeFeedKey(gchar *keyprefix);

/* preferences configuration methods */

gboolean 	getBooleanConfValue(gchar *valuename);
gchar *		getStringConfValue(gchar *valuename);
gint		getNumericConfValue(gchar  *valuename);

void 	setBooleanConfValue(gchar *valuename, gboolean value);
void	setStringConfValue(gchar *valuename, gchar *value);
void	setNumericConfValue(gchar  *valuename, gint value);

#endif
