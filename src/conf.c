/**
 * @file conf.c Liferea configuration (gconf access)
 *
 * Copyright (C) 2003-2009 Lars Lindner <lars.lindner@gmail.com>
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

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <libxml/uri.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "net.h"
#include "update.h"
#include "ui/liferea_shell.h"
#include "ui/ui_tray.h"

#define MAX_GCONF_PATHLEN	256

#define PATH		"/apps/liferea"

static GConfClient	*client;

/* Function prototypes */
static void conf_proxy_reset_settings_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);
static void conf_tray_settings_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);
static void conf_toolbar_style_settings_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);

static gboolean
is_gconf_error (GError **err)
{
	if (*err) {
		g_warning ("%s\n", (*err)->message);
		g_error_free (*err);
		*err = NULL;
		return TRUE;
	}
	
	return FALSE;
}

/* called once on startup */
void
conf_init (void)
{	
	/* call g_type_init(), GConf for some reason refuses to do so itself 
	   and we use GConf before initializing GTK which would call g_type_init() itself */
	g_type_init ();
	
	/* initialize GConf client */
	client = gconf_client_get_default ();
	gconf_client_add_dir (client, PATH, GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_add_dir (client, "/apps/liferea/proxy", GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_add_dir (client, "/system/http_proxy", GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_NONE, NULL);
	
	gconf_client_notify_add (client, "/apps/liferea/proxy", conf_proxy_reset_settings_cb, NULL, NULL, NULL);
	gconf_client_notify_add (client, "/system/http_proxy", conf_proxy_reset_settings_cb, NULL, NULL, NULL);
	gconf_client_notify_add (client, "/desktop/gnome/interface/toolbar_style", conf_toolbar_style_settings_cb, NULL, NULL, NULL);
	gconf_client_notify_add (client, SHOW_TRAY_ICON, conf_tray_settings_cb, NULL, NULL, NULL);
	
	/* Load settings into static buffers */
	conf_proxy_reset_settings_cb (NULL, 0, NULL, NULL);
}

void
conf_deinit (void)
{
	g_object_unref (client);
}

/* maybe called several times to reload configuration */
void
conf_load (void)
{
	gint	maxitemcount;
	gchar *downloadPath;
	
	/* check if important preferences exist... */
	
	if (!conf_get_int_value (DEFAULT_MAX_ITEMS, &maxitemcount))
		conf_set_int_value (DEFAULT_MAX_ITEMS, 100);
	
	if (!conf_get_str_value (ENCLOSURE_DOWNLOAD_PATH, &downloadPath))
		conf_set_str_value (ENCLOSURE_DOWNLOAD_PATH, g_getenv ("HOME"));
	g_free (downloadPath);
}

static void
conf_tray_settings_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	GConfValue *value;
	
	if (entry) {
		value = gconf_entry_get_value (entry);
		if (value && value->type == GCONF_VALUE_BOOL)
			ui_tray_enable (gconf_value_get_bool (value));
	}
}

static void
conf_toolbar_style_settings_cb (GConfClient *client,
                                guint cnxn_id,
                                GConfEntry *entry,
                                gpointer user_data) 
{
	gchar *style = conf_get_toolbar_style ();

	if (style) {
		liferea_shell_set_toolbar_style (style);
		g_free (style);
	}
}

static void
conf_proxy_reset_settings_cb (GConfClient *client,
                              guint cnxn_id,
                              GConfEntry *entry,
                              gpointer user_data)
{
	gchar		*proxyname, *proxyusername, *proxypassword, *tmp;
	gboolean	gnomeUseProxy;
	guint		proxyport;
	gint		proxydetectmode;
	gboolean	proxyuseauth;
	xmlURIPtr 	uri;
	
	proxyname = NULL;
	proxyport = 0;
	proxyusername = NULL;
	proxypassword = NULL;

	conf_get_int_value (PROXY_DETECT_MODE, &proxydetectmode);
	switch (proxydetectmode) {
		default:
		case 0:
			debug0 (DEBUG_CONF, "proxy auto detect is configured");

			/* first check for a configured GNOME proxy, note: older
			   GNOME versions do use the boolean flag GNOME_USE_PROXY
			   while newer ones use the string key GNOME_PROXY_MODE */
			conf_get_str_value (GNOME_PROXY_MODE, &tmp);
			conf_get_bool_value (GNOME_USE_PROXY, &gnomeUseProxy);
			gnomeUseProxy = gnomeUseProxy || g_str_equal (tmp, "manual");
			g_free (tmp);				
			
			/* first check for a configured GNOME proxy */
			if (gnomeUseProxy) {
				conf_get_str_value (GNOME_PROXY_HOST, &proxyname);
				conf_get_int_value (GNOME_PROXY_PORT, &proxyport);
				debug2 (DEBUG_CONF, "using GNOME configured proxy: \"%s\" port \"%d\"", proxyname, proxyport);
				conf_get_bool_value (GNOME_PROXY_USEAUTH, &proxyuseauth);
				if (proxyuseauth) {
					conf_get_str_value (GNOME_PROXY_USER, &proxyusername);
					conf_get_str_value (GNOME_PROXY_PASSWD, &proxypassword);
				}
			} else {
				/* otherwise there could be a proxy specified in the environment 
				   the following code was derived from SnowNews' setup.c */
				if (g_getenv("http_proxy")) {
					/* The pointer returned by getenv must not be altered.
					   What about mentioning this in the manpage of getenv? */
					debug0 (DEBUG_CONF, "using proxy from environment");
					do {
						uri = xmlParseURI (BAD_CAST g_getenv ("http_proxy"));
						if (uri == NULL) {
							debug0 (DEBUG_CONF, "parsing URI in $http_proxy failed!");
							break;
						}
						if (uri->server == NULL) {
							debug0 (DEBUG_CONF, "could not determine proxy name from $http_proxy!");
							xmlFreeURI (uri);
							break;
						}
						proxyname = g_strdup (uri->server);
						proxyport = (uri->port == 0) ? 3128 : uri->port;
						if (uri->user) {
							tmp = strtok (uri->user, ":");
							tmp = strtok (NULL, ":");
							if (tmp) {
								proxyusername = g_strdup (uri->user);
								proxypassword = g_strdup (tmp);
							}
						}
						xmlFreeURI (uri);
					} while (FALSE);
				}
			}
			if (!proxyname)
				debug0 (DEBUG_CONF, "no proxy GNOME of $http_proxy configuration found...");
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
	
	debug4 (DEBUG_CONF, "Proxy settings are now %s:%d %s:%s", proxyname != NULL ? proxyname : "NULL", proxyport,
		  proxyusername != NULL ? proxyusername : "NULL",
		  proxypassword != NULL ? proxypassword : "NULL");
		  
	network_set_proxy (proxyname, proxyport, proxyusername, proxypassword);
}

/*----------------------------------------------------------------------*/
/* generic configuration access methods					*/
/*----------------------------------------------------------------------*/

void
conf_set_bool_value (const gchar *valuename, gboolean value)
{
	GError		*err = NULL;
	GConfValue	*gcv;
	
	g_assert (valuename != NULL);
	
	gcv = gconf_value_new (GCONF_VALUE_BOOL);
	gconf_value_set_bool (gcv, value);
	gconf_client_set (client, valuename, gcv, &err);
	gconf_value_free (gcv);
	is_gconf_error (&err);
}

gboolean
conf_get_bool_value (const gchar *valuename, gboolean *value)
{
	GConfValue	*gcv;

	g_assert (valuename != NULL);

	gcv = gconf_client_get (client, valuename, NULL);
	if (gcv) {
		*value = gconf_value_get_bool (gcv);
		gconf_value_free (gcv);
		return TRUE;
	} else {
		*value = FALSE;
		return FALSE;
	}
}

void
conf_set_str_value (const gchar *valuename, const gchar *value)
{
	GError		*err = NULL;
	GConfValue	*gcv;
	
	g_assert (valuename != NULL);
	
	gcv = gconf_value_new (GCONF_VALUE_STRING);
	gconf_value_set_string (gcv, value);
	gconf_client_set (client, valuename, gcv, &err);
	gconf_value_free (gcv);
	is_gconf_error (&err);
}

gboolean
conf_get_str_value (const gchar *valuename, gchar **value)
{
	GConfValue	*gcv;

	g_assert (valuename != NULL);
		
	gcv = gconf_client_get (client, valuename, NULL);
	if (gcv) {
		*value = g_strdup (gconf_value_get_string (gcv));
		gconf_value_free (gcv);
		return TRUE;
	} else {
		*value = g_strdup ("");
		return FALSE;
	}
}

void
conf_set_int_value (const gchar *valuename, gint value)
{
	GError		*err = NULL;
	GConfValue	*gcv;
	
	g_assert (valuename != NULL);
	debug2 (DEBUG_CONF, "Setting %s to %d", valuename, value);
	gcv = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (gcv, value);
	gconf_client_set (client, valuename, gcv, &err);
	is_gconf_error (&err);
	gconf_value_free (gcv);
}

gboolean
conf_get_int_value (const gchar *valuename, gint *value)
{
	GConfValue	*gcv;

	g_assert (valuename != NULL);
		
	gcv = gconf_client_get (client, valuename, NULL);
	if (gcv) {
		*value = gconf_value_get_int (gcv);
		gconf_value_free (gcv);
		return TRUE;
	} else {
		*value = 0;
		return FALSE;	
	}
}

gchar *
conf_get_toolbar_style(void) 
{
	gchar *style;

	conf_get_str_value (TOOLBAR_STYLE, &style);

	/* check if we don't override the toolbar style */
	if (strcmp(style, "") == 0) {
		g_free (style);
		conf_get_str_value ("/desktop/gnome/interface/toolbar_style", &style);
	}
	return style;
}
