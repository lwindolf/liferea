/**
 * @file conf.h Liferea configuration (gconf access)
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

/* browsing settings */
#define BROWSE_INSIDE_APPLICATION	"/apps/liferea/browse-inside-application"
#define BROWSE_KEY_SETTING		"/apps/liferea/browse-key-setting"
#define BROWSER_ID			"/apps/liferea/browser_id"
#define BROWSER_PLACE			"/apps/liferea/browser_place"
#define BROWSER_COMMAND			"/apps/liferea/browser"

#define GNOME_DEFAULT_BROWSER_COMMAND	"gnome-open '%s'"
#define DEFAULT_BROWSER_COMMAND		"mozilla '%s'"

#define DEFAULT_FONT			"/desktop/gnome/interface/document_font_name"
#define USER_FONT			"/apps/liferea/browser-font"
#define REFOCUS_TIMEOUT			"/apps/liferea/refocus-timeout"
#define DISABLE_JAVASCRIPT		"/apps/liferea/disable-javascript"
#define SOCIAL_BM_SITE			"/apps/liferea/social-bm-site"
#define SOCIAL_BM_HIDE_LINK		"/apps/liferea/social-bm-hide-link"
#define SEARCH_ENGINE			"/apps/liferea/search-engine"
#define SEARCH_ENGINE_HIDE_LINK		"/apps/liferea/search-engine-hide-link"

/* enclosure handling */
#define ENCLOSURE_DOWNLOAD_TOOL		"/apps/liferea/enclosure-download-tool"
#define ENCLOSURE_DOWNLOAD_PATH		"/apps/liferea/enclosure-download-path"

/* item list settings */
#define DATE_FORMAT			"/apps/liferea/date-format"

/* feed handling settings */
#define DEFAULT_MAX_ITEMS		"/apps/liferea/maxitemcount"
#define DEFAULT_UPDATE_INTERVAL		"/apps/liferea/default-update-interval"
#define STARTUP_FEED_ACTION		"/apps/liferea/startup_feed_action"
#define UPDATE_THREAD_CONCURRENCY	"/apps/liferea/update-thread-concurrency"
#define ENABLE_FETCH_RETRIES		"/apps/liferea/enable-fetch-retries"

/* folder handling settings */
#define FOLDER_DISPLAY_MODE		"/apps/liferea/folder-display-mode"
#define FOLDER_DISPLAY_HIDE_READ	"/apps/liferea/folder-display-hide-read"

/* GUI settings and persistency values */
#define SHOW_TRAY_ICON			"/apps/liferea/trayicon"
#define SHOW_NEW_COUNT_IN_TRAY		"/apps/liferea/trayicon-new-count"
#define DONT_MINIMIZE_TO_TRAY		"/apps/liferea/dont-minimize-to-tray"
#define SHOW_POPUP_WINDOWS		"/apps/liferea/show-popup-windows"
#define POPUP_PLACEMENT			"/apps/liferea/popup-placement"
#define DISABLE_MENUBAR			"/apps/liferea/disable-menubar"
#define DISABLE_TOOLBAR			"/apps/liferea/disable-toolbar"
#define TOOLBAR_STYLE			"/apps/liferea/toolbar-style"
#define LAST_WINDOW_X			"/apps/liferea/last-window-x"
#define LAST_WINDOW_Y			"/apps/liferea/last-window-y"
#define LAST_WINDOW_WIDTH		"/apps/liferea/last-window-width"
#define LAST_WINDOW_HEIGHT		"/apps/liferea/last-window-height"
#define LAST_WINDOW_MAXIMIZED		"/apps/liferea/last-window-maximized"
#define LAST_VPANE_POS			"/apps/liferea/last-vpane-pos"
#define LAST_HPANE_POS			"/apps/liferea/last-hpane-pos"
#define LAST_WPANE_POS			"/apps/liferea/last-wpane-pos"
#define LAST_ZOOMLEVEL			"/apps/liferea/last-zoomlevel"

/* networking settings */
#define NETWORK_TIMEOUT			"/apps/liferea/network-timeout"
#define PROXY_DETECT_MODE		"/apps/liferea/proxy/detect-mode"
#define PROXY_HOST			"/apps/liferea/proxy/host"
#define PROXY_PORT			"/apps/liferea/proxy/port"
#define PROXY_USEAUTH			"/apps/liferea/proxy/use_authentication"
#define PROXY_USER			"/apps/liferea/proxy/authentication_user"
#define PROXY_PASSWD			"/apps/liferea/proxy/authentication_password"
#define GNOME_USE_PROXY			"/system/http_proxy/use_http_proxy"	/* old GNOME proxy enabling key */
#define GNOME_PROXY_MODE		"/system/proxy/mode"			/* new GNOME proxy mode key */
#define GNOME_PROXY_HOST		"/system/http_proxy/host"
#define GNOME_PROXY_PORT		"/system/http_proxy/port"
#define GNOME_PROXY_USEAUTH		"/system/http_proxy/use_authentication"
#define GNOME_PROXY_USER		"/system/http_proxy/authentication_user"
#define GNOME_PROXY_PASSWD		"/system/http_proxy/authentication_password"

/* other settings */
#define DISABLE_DBUS			"/apps/liferea/disable-dbus"

/* initializing methods */
void	conf_init (void);
void	conf_deinit (void);
void	conf_load (void);

/* preferences access methods */

/**
 * Retrieves the value of the given boolean configuration key.
 *
 * @param key	the configuration key
 *
 * @returns boolean value (FALSE on failure)
 */
#define getBooleanConfValue(key) conf_get_bool_value(key)
gboolean conf_get_bool_value (const gchar *key);

/**
 * Retrieves the value of the given string configuration key.
 *
 * @param key	the configuration key
 *
 * @returns string value (NULL on failure, to be free'd using g_free)
 */
#define getStringConfValue(key) conf_get_str_value(key)
gchar *	conf_get_str_value (const gchar *key);

/**
 * Retrieves the value of the given integer configuration key.
 *
 * @param key	the configuration key
 *
 * @returns integer value (0 on failure)
 */
#define getNumericConfValue(key) conf_get_int_value(key)
gint conf_get_int_value (const gchar *key);

/**
 * Sets the value of the given boolean configuration key.
 * 
 * @param key	the configuration key
 * @param value	the new boolean value
 */
#define setBooleanConfValue(key,value) conf_set_bool_value(key,value)
void conf_set_bool_value (const gchar *key, gboolean value);

/**
 * Sets the value of the given string configuration key.
 * The given value will not be free'd after setting it!
 *
 * @param key	the configuration key
 * @param value	the new string value
 */
#define setStringConfValue(key,value) conf_set_str_value(key,value)
void conf_set_str_value (const gchar *key, const gchar *value);

/**
 * Sets the value of the given integer configuration key
 *
 * @param key	the configuration key
 * @param value	the new integer value
 */
#define setNumericConfValue(key,value) conf_set_int_value(key,value)
void conf_set_int_value (const gchar *key, gint value);

/**
 * Returns the current toolbar configuration.
 *
 * @returns a string (to be free'd using g_free)
 */
gchar * conf_get_toolbar_style (void);

#endif
