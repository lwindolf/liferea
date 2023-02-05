/**
 * @file conf.h  Liferea configuration (GSettings access)
 *
 * Copyright (C) 2011 Mikel Olasagasti Uranga <mikel@olasagasti.info>
 * Copyright (C) 2003-2017 Lars Windolf <lars.windolf@gmx.de>
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
#include <gio/gio.h>

/* browsing settings */
#define BROWSE_INSIDE_APPLICATION	"browse-inside-application"
#define BROWSE_KEY_SETTING		"browse-key-setting"
#define BROWSER_ID			"browser-id"
#define BROWSER_COMMAND			"browser"

#define DEFAULT_VIEW_MODE		"default-view-mode"
#define DEFER_DELETE_MODE               "defer-delete-mode"

#define DEFAULT_FONT			"document-font-name"
#define USER_FONT			"browser-font"
#define DISABLE_JAVASCRIPT		"disable-javascript"
#define SOCIAL_BM_SITE			"social-bm-site"
#define ENABLE_PLUGINS			"enable-plugins"
#define ENABLE_ITP			"enable-itp"
#define ENABLE_READER_MODE		"enable-reader-mode"

/* enclosure handling */
#define DOWNLOAD_CUSTOM_COMMAND 	"download-custom-command"
#define DOWNLOAD_TOOL			"download-tool"
#define DOWNLOAD_USE_CUSTOM_COMMAND	"download-use-custom-command"

/* feed handling settings */
#define DEFAULT_MAX_ITEMS		"maxitemcount"
#define DEFAULT_UPDATE_INTERVAL		"default-update-interval"
#define STARTUP_FEED_ACTION		"startup-feed-action"

/* folder handling settings */
#define FOLDER_DISPLAY_MODE		"folder-display-mode"
#define FOLDER_DISPLAY_HIDE_READ	"folder-display-hide-read"
#define REDUCED_FEEDLIST		"reduced-feedlist"

/* GUI settings and persistency values */
#define CONFIRM_MARK_ALL_READ 		"confirm-mark-all-read"
#define DISABLE_TOOLBAR			"disable-toolbar"
#define TOOLBAR_STYLE			"toolbar-style"
#define LAST_WINDOW_STATE		"last-window-state"
#define LAST_WINDOW_X			"last-window-x"
#define LAST_WINDOW_Y			"last-window-y"
#define LAST_WINDOW_WIDTH		"last-window-width"
#define LAST_WINDOW_HEIGHT		"last-window-height"
#define LAST_WINDOW_MAXIMIZED		"last-window-maximized"
#define LAST_VPANE_POS			"last-vpane-pos"
#define LAST_HPANE_POS			"last-hpane-pos"
#define LAST_WPANE_POS			"last-wpane-pos"
#define LAST_ZOOMLEVEL			"last-zoomlevel"
#define LAST_NODE_SELECTED		"last-node-selected"
#define LIST_VIEW_COLUMN_ORDER		"list-view-column-order"

/* networking settings */
#define PROXY_DETECT_MODE		"proxy-detect-mode"
#define PROXY_HOST			"proxy-host"
#define PROXY_PORT			"proxy-port"
#define PROXY_USEAUTH			"proxy-use-authentication"
#define PROXY_USER			"proxy-authentication-user"
#define PROXY_PASSWD			"proxy-authentication-password"
#define DO_NOT_TRACK			"do-not-track"
#define INTRANET_CONNECTIVITY		"intranet-connectivity"

/* initializing methods */
void	conf_init (void);
void	conf_deinit (void);

/* preferences access methods */

#define conf_get_bool_value(key, value) conf_get_bool_value_from_schema (NULL, key, value)
#define conf_get_str_value(key, value) conf_get_str_value_from_schema (NULL, key, value)
#define conf_get_strv_value(key, value) conf_get_strv_value_from_schema (NULL, key, value)
#define conf_get_int_value(key, value) conf_get_int_value_from_schema (NULL, key, value)
#define conf_get_enum_value(key, value) conf_get_enum_value_from_schema (NULL, key, value)


/**
 * Returns true if the key is defined in the schema for the given gsettings.
 *
 * @param gsettings	gsettings schema to use
 * @param key	the configuration key
 *
 * @returns TRUE if the configuration key is defined
 */
gboolean conf_schema_has_key (GSettings *gsettings, const gchar *key);

/**
 * Retrieves the value of the given boolean configuration key.
 *
 * @param gsettings	gsettings schema to use
 * @param key	the configuration key
 * @param value the value, if the function returned FALSE it's always FALSE
 *
 * @returns TRUE if the configuration key was found
 */
gboolean conf_get_bool_value_from_schema (GSettings *gsettings, const gchar *key, gboolean *value);

/**
 * Retrieves the value of the given string configuration key.
 * The string has to be freed by the caller.
 *
 * @param gsettings	gsettings schema to use
 * @param key	the configuration key
 * @param value the value, if the function returned FALSE an empty string
 *
 * @returns TRUE if the configuration key was found
 */
gboolean conf_get_str_value_from_schema (GSettings *gsettings,const gchar *key, gchar **value);

/**
 * Retrieves the value of the given string array configuration key.
 * The string array has to be freed by the caller.
 *
 * @param gsettings	gsettings schema to use
 * @param key	the configuration key
 * @param value the value, if the function returned FALSE an empty string
 *
 * @returns TRUE if the configuration key was found
 */
gboolean conf_get_strv_value_from_schema (GSettings *gsettings,const gchar *key, gchar ***value);

/**
 * Retrieves the value of the given integer configuration key.
 *
 * @param gsettings	gsettings schema to use
 * @param key	the configuration key
 * @param value the value, if the function returned FALSE it's always 0
 *
 * @returns TRUE if the configuration key was found
 */
gboolean conf_get_int_value_from_schema (GSettings *gsettings, const gchar *key, gint *value);

/**
 * Retrieves the value of the given enum configuration key.
 *
 * @param gsettings	gsettings schema to use
 * @param key	the configuration key
 * @param value the value, if the function returned FALSE it's always 0
 *
 * @returns TRUE if the configuration key was found
 */
gboolean conf_get_enum_value_from_schema (GSettings *gsettings, const gchar *key, gint *value);

/**
 * Sets the value of the given boolean configuration key.
 *
 * @param key	the configuration key
 * @param value	the new boolean value
 */
void conf_set_bool_value (const gchar *key, gboolean value);

/**
 * Sets the value of the given string configuration key.
 * The given value will not be free'd after setting it!
 *
 * @param key	the configuration key
 * @param value	the new string value
 */
void conf_set_str_value (const gchar *key, const gchar *value);

/**
 * Sets the value of the given string configuration key.
 * The given value will not be free'd after setting it!
 *
 * @param key	the configuration key
 * @param value	the new string value
 */
void conf_set_strv_value (const gchar *key, const gchar **value);

/**
 * Sets the value of the given integer configuration key
 *
 * @param key	the configuration key
 * @param value	the new integer value
 */
void conf_set_int_value (const gchar *key, gint value);

/**
 * Sets the value of the given enum configuration key
 *
 * @param key	the configuration key
 * @param value	the new enum (as integer) value
 */
void conf_set_enum_value (const gchar *key, gint value);


/**
 * Returns the current toolbar configuration.
 *
 * @returns a string (to be free'd using g_free)
 */
gchar * conf_get_toolbar_style (void);

/**
 * Get the current system default font from desktop schema
 *
 * @param value the value, if the function returned FALSE it's always 0
 *
 * @returns TRUE if the configuration key was found
*/
gboolean conf_get_default_font (gchar **value);

/**
 * Find out wether a dark theme preference is active right now.
 *
 * @returns TRUE if a dark theme preference is active
 */
gboolean conf_get_dark_theme (void);

/**
 * Connect to a signal in the default GSettings object
 *
 * @param signal the signal to connect to
 * @param cb	 callback to invoke when the signal is emitted
 * @param data	 user data to pass to the callback
 */
void conf_signal_connect (const gchar *signal, GCallback cb, gpointer data);

/**
 * conf_bind:
 * @key: the configuration key
 * @object: a GObject
 * @property: the object's property to bind
 * @flags: binding flags
 *
 * This is a convenience function that calls g_settings_bind with Liferea settings.
 */
void conf_bind (const gchar *key, gpointer object, const gchar *property, GSettingsBindFlags flags);
#endif
