/**
 * @file conf.c Liferea configuration (GSettings access)
 *
 * Copyright (C) 2011 Mikel Olasagasti Uranga <mikel@olasagasti.info>
 * Copyright (C) 2003-2022 Lars Windolf <lars.windolf@gmx.de>
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
#include <webkit2/webkit2.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "net.h"
#include "render.h"
#include "ui/liferea_shell.h"

#define MAX_GCONF_PATHLEN	256

#define LIFEREA_SCHEMA_NAME		"net.sf.liferea"
#define DESKTOP_SCHEMA_NAME		"org.gnome.desktop.interface"
#define FDO_SCHEMA_NAME			"org.freedesktop.appearance"

static GSettings *settings;
static GSettings *desktop_settings;
static GSettings *fdo_settings;

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

gboolean
conf_get_dark_theme (void)
{
	gboolean dark = FALSE;
	
	if (fdo_settings) {
		gint scheme;

		if (conf_schema_has_key (fdo_settings, "color-scheme")) {
			conf_get_int_value_from_schema (fdo_settings, "color-scheme", &scheme);
			debug (DEBUG_CONF, "FDO reports color-schema code '%d'", scheme);
			if (1 == scheme)
				dark = FALSE;
			if (0 == scheme || 2 == scheme)
				dark = TRUE;
		}
	} else {
		gchar *scheme = NULL;

		if (conf_schema_has_key (desktop_settings, "color-scheme")) {
			conf_get_str_value_from_schema (desktop_settings, "color-scheme", &scheme);
			if (scheme) {
				debug (DEBUG_CONF, "GNOME reports color-schema '%s'", scheme);
				dark = g_str_equal (scheme, "prefer-dark");
				g_free (scheme);
			}
		}
	}

	debug (DEBUG_CONF, "Determined dark theme mode to be %d", dark);
	return dark;
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
	gint	mode;

	conf_get_int_value (PROXY_DETECT_MODE, &mode);
	network_set_proxy (mode);
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
conf_set_strv_value (const gchar *key, const gchar **value)
{
	g_assert (key != NULL);
	g_settings_set_strv (settings, key, value);
}

void
conf_set_int_value (const gchar *key, gint value)
{
	g_assert (key != NULL);
	debug (DEBUG_CONF, "Setting %s to %d", key, value);
	g_settings_set_int (settings, key, value);
}

void
conf_set_enum_value (const gchar *key, gint value)
{
	g_assert (key != NULL);
	g_settings_set_enum (settings, key, value);
}

gchar *
conf_get_toolbar_style(void)
{
	gchar *style;

	conf_get_str_value (TOOLBAR_STYLE, &style);

	/* check if we don't override the toolbar style */
	if (strcmp (style, "") == 0) {
		g_free (style);
		conf_get_str_value_from_schema (desktop_settings, "toolbar-style", &style);
	}
	return style;
}

gboolean
conf_schema_has_key (GSettings *gsettings, const gchar *key)
{
	g_assert (gsettings != NULL);
	g_assert (key != NULL);

	GSettingsSchema *schema = NULL;
	gboolean has_key = FALSE;

	g_object_get (gsettings, "settings-schema", &schema, NULL);
	if (schema) {
		has_key = g_settings_schema_has_key (schema, key);
		g_settings_schema_unref (schema);
	}
	return has_key;
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
conf_get_strv_value_from_schema (GSettings *gsettings, const gchar *key, gchar ***value)
{
	g_assert (key != NULL);
	g_assert (value != NULL);

	if (gsettings == NULL)
		gsettings = settings;
	*value = g_settings_get_strv (gsettings, key);
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
conf_get_enum_value_from_schema (GSettings *gsettings, const gchar *key, gint *value)
{
	g_assert (key != NULL);
	g_assert (value != NULL);

	if (gsettings == NULL)
		gsettings = settings;
	*value = g_settings_get_enum (gsettings,key);
	return (NULL != value);
}


gboolean
conf_get_default_font (gchar **value)
{
	g_assert (value != NULL);

	if (desktop_settings)
		*value = g_strdup (g_settings_get_string (desktop_settings, DEFAULT_FONT));
	return (NULL != value);
}

void
conf_signal_connect (const gchar *signal, GCallback cb, gpointer data)
{
	g_signal_connect (settings, signal, cb, data);
}

void
conf_bind (const gchar *key, gpointer object, const gchar *property, GSettingsBindFlags flags)
{
	g_assert (settings);
	g_settings_bind (settings, key, object, property, flags);
}

/* called once on startup */
void
conf_init (void)
{
	GSettingsSchemaSource *source;

	/* ensure we migrated from gconf to gsettings */
	conf_ensure_migrated (LIFEREA_SCHEMA_NAME);

	/* initialize GSettings client */
	settings = g_settings_new (LIFEREA_SCHEMA_NAME);
	desktop_settings = g_settings_new (DESKTOP_SCHEMA_NAME);

	/* provide freedesktop.org settings for non-GNOME desktops */
	source = g_settings_schema_source_get_default ();
	if (g_settings_schema_source_lookup (source, FDO_SCHEMA_NAME, TRUE))
		fdo_settings = g_settings_new (FDO_SCHEMA_NAME);

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
	if (fdo_settings)
		g_object_unref (fdo_settings);
}

