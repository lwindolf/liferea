/**
 * @file conf.c Liferea configuration (GSettings access)
 *
 * Copyright (C) 2011 Mikel Olasagasti Uranga <mikel@olasagasti.info>
 * Copyright (C) 2003-2015 Lars Windolf <lars.windolf@gmx.de>
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

#include <libxml/uri.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "net.h"
#include "update.h"
#include "ui/liferea_shell.h"

#define MAX_GCONF_PATHLEN	256

#define LIFEREA_SCHEMA_NAME		"net.sf.liferea"
#define DESKTOP_SCHEMA_NAME		"org.gnome.desktop.interface"

static GSettings *settings;
static GSettings *desktop_settings;

/* Function prototypes */
static void conf_proxy_reset_settings_cb(GSettings *settings, guint cnxn_id, gchar *key, gpointer user_data);
static void conf_toolbar_style_settings_cb(GSettings *settings, guint cnxn_id, gchar *key, gpointer user_data);

static void
conf_ensure_migrated (const gchar *name)
{
	gboolean needed = TRUE;
	GKeyFile *kf;
	gchar **list;
	gsize i, n;

	kf = g_key_file_new ();

	g_key_file_load_from_data_dirs (kf, "gsettings-data-convert",
					NULL, G_KEY_FILE_NONE, NULL);
	list = g_key_file_get_string_list (kf, "State", "converted", &n, NULL);

	if (list) {
		for (i = 0; i < n; i++) {
			if (strcmp (list[i], name) == 0) {
				needed = FALSE;
				break;
			}
		}
		g_strfreev (list);
	}

	g_key_file_free (kf);

	if (needed)
		g_spawn_command_line_sync ("gsettings-data-convert",
						NULL, NULL, NULL, NULL);
}

/* called once on startup */
void
conf_init (void)
{	
	/* ensure we migrated from gconf to gsettings */
	conf_ensure_migrated (LIFEREA_SCHEMA_NAME);

	/* initialize GSettings client */
	settings = g_settings_new (LIFEREA_SCHEMA_NAME);
	desktop_settings = g_settings_new(DESKTOP_SCHEMA_NAME);

	g_signal_connect (
		desktop_settings,
		"changed::" TOOLBAR_STYLE,
		G_CALLBACK (conf_toolbar_style_settings_cb),
		NULL
	);

	g_signal_connect (
		settings,
		"changed::" PROXY_DETECT_MODE,
		G_CALLBACK (conf_proxy_reset_settings_cb),
		NULL
	);
	g_signal_connect (
		settings,
		"changed::" PROXY_HOST,
		G_CALLBACK (conf_proxy_reset_settings_cb),
		NULL
	);
	g_signal_connect (
		settings,
		"changed::" PROXY_PORT,
		G_CALLBACK (conf_proxy_reset_settings_cb),
		NULL
	);
	g_signal_connect (
		settings,
		"changed::" PROXY_USEAUTH,
		G_CALLBACK (conf_proxy_reset_settings_cb),
		NULL
	);
	g_signal_connect (
		settings,
		"changed::" PROXY_USER,
		G_CALLBACK (conf_proxy_reset_settings_cb),
		NULL
	);
	g_signal_connect (
		settings,
		"changed::" PROXY_PASSWD,
		G_CALLBACK (conf_proxy_reset_settings_cb),
		NULL
	);

	/* Load settings into static buffers */
	conf_proxy_reset_settings_cb (NULL, 0, NULL, NULL);
}

void
conf_deinit (void)
{
	g_object_unref (settings);
	g_object_unref (desktop_settings);
}

static void
conf_toolbar_style_settings_cb (GSettings *settings,
                                guint cnxn_id,
                                gchar *key,
                                gpointer user_data)
{
	gchar *style = conf_get_toolbar_style ();

	if (style) {
		liferea_shell_set_toolbar_style (style);
		g_free (style);
	}
}

static void
conf_proxy_reset_settings_cb (GSettings *settings,
                              guint cnxn_id,
                              gchar *key,
                              gpointer user_data)
{
	gchar		*proxyname, *proxyusername, *proxypassword;
	gint		proxyport;
	gint		proxydetectmode;
	gboolean	proxyuseauth;

	proxyname = NULL;
	proxyport = 0;
	proxyusername = NULL;
	proxypassword = NULL;

	conf_get_int_value (PROXY_DETECT_MODE, &proxydetectmode);
	switch (proxydetectmode) {
		default:
		case 0:
			debug0 (DEBUG_CONF, "proxy auto detect is configured");
			/* nothing to do, all done by libproxy inside libsoup */
			break;
		case 1:
			debug0 (DEBUG_CONF, "proxy is disabled by user");
			/* nothing to do */
			break;
		case 2:
			debug0 (DEBUG_CONF, "manual proxy is configured");

			conf_get_str_value (PROXY_HOST, &proxyname);
			conf_get_int_value (PROXY_PORT, &proxyport);
			conf_get_bool_value (PROXY_USEAUTH, &proxyuseauth);
			if (proxyuseauth) {
				conf_get_str_value (PROXY_USER, &proxyusername);
				conf_get_str_value (PROXY_PASSWD, &proxypassword);
			}
			break;
	}
	debug4 (DEBUG_CONF, "Manual proxy settings are now %s:%d %s:%s", proxyname != NULL ? proxyname : "NULL", proxyport,
		  proxyusername != NULL ? proxyusername : "NULL",
		  proxypassword != NULL ? proxypassword : "NULL");

	network_set_proxy (proxyname, proxyport, proxyusername, proxypassword);
}

/*----------------------------------------------------------------------*/
/* generic configuration access methods					*/
/*----------------------------------------------------------------------*/

void
conf_set_bool_value (const gchar *key, gboolean value)
{
	g_assert (key != NULL);
	g_settings_set_boolean (settings, key, value);
}

void
conf_set_str_value (const gchar *key, const gchar *value)
{
	g_assert (key != NULL);
	g_settings_set_string (settings, key, value);
}

void
conf_set_int_value (const gchar *key, gint value)
{
	g_assert (key != NULL);
	debug2 (DEBUG_CONF, "Setting %s to %d", key, value);
	g_settings_set_int (settings, key, value);
}

gchar *
conf_get_toolbar_style(void)
{
	gchar *style;

	conf_get_str_value (TOOLBAR_STYLE, &style);

	/* check if we don't override the toolbar style */
	if (strcmp(style, "") == 0) {
		g_free (style);
		conf_get_str_value_from_schema (desktop_settings,"toolbar-style", &style);
	}
	return style;
}

gboolean
conf_get_bool_value_from_schema (GSettings *gsettings, const gchar *key, gboolean *value)
{
	g_assert (key != NULL);
	g_assert (value != NULL);

	if (gsettings == NULL)
		gsettings = settings;
	*value = g_settings_get_boolean (gsettings,key);
	return *value;
}

gboolean
conf_get_str_value_from_schema (GSettings *gsettings, const gchar *key, gchar **value)
{
	g_assert (key != NULL);
	g_assert (value != NULL);

	if (gsettings == NULL)
		gsettings = settings;
	*value = g_settings_get_string (gsettings, key);
	return (NULL != value);
}

gboolean
conf_get_int_value_from_schema (GSettings *gsettings, const gchar *key, gint *value)
{
	g_assert (key != NULL);
	g_assert (value != NULL);

	if (gsettings == NULL)
		gsettings = settings;
	*value = g_settings_get_int (gsettings,key);
	return (NULL != value);
}

gboolean
conf_get_default_font_from_schema (const gchar *key, gchar **value)
{
	g_assert (key != NULL);
	g_assert (value != NULL);

	if (desktop_settings)
		*value = g_strdup (g_settings_get_string (desktop_settings, key));
	return (NULL != value);
}

void
conf_signal_connect (const gchar *signal, GCallback cb, gpointer data)
{
	g_signal_connect (settings, signal, cb, data);
}
