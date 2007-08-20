/**
 * @file conf.c Liferea configuration (gconf access)
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <libxml/uri.h>
#include <string.h>
#include <time.h>
#include "support.h"
#include "callbacks.h"
#include "update.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "ui/ui_tray.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"

#define MAX_GCONF_PATHLEN	256

#define PATH		"/apps/liferea"

#define HOMEPAGE	"http://liferea.sf.net/"

static GConfClient	*client;

/* configuration values for the SnowNews HTTP code used from within netio.c */
int	NET_TIMEOUT = 30;
char 	*useragent = NULL;
char	*proxyname = NULL;
char	*proxyusername = NULL;
char	*proxypassword = NULL;
int	proxyport = 0;

/* Function prototypes */
static void conf_proxy_reset_settings_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);
static void conf_tray_settings_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);
static void conf_toolbar_style_settings_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);

static gboolean is_gconf_error(GError **err) {

	if(*err != NULL) {
		g_warning("%s\n", (*err)->message);
		g_error_free(*err);
		*err = NULL;
		return TRUE;
	}
	
	return FALSE;
}

/* called once on startup */
void conf_init() {
	
	/* has to be called for multithreaded programs */
	xmlInitParser();
	
	/* the following code was copied from SnowNews and adapted to build
	   a Liferea user agent... */
	
	/* Construct the User-Agent string of Liferea. This is done here in program init,
	   because we need to do it exactly once and it will never change while the program
	   is running. */
	if (g_getenv("LANG") != NULL) {
		/* e.g. Liferea/0.3.8 (Linux; de_DE; (http://liferea.sf.net/) */
		useragent = g_strdup_printf("Liferea/%s (%s; %s; %s)", VERSION, OSNAME, g_getenv("LANG"), HOMEPAGE);
	} else {
		/* "Liferea/" + VERSION + "(" OS + "; " + HOMEPAGE + ")" */
		useragent = g_strdup_printf("Liferea/%s (%s; %s)", VERSION, OSNAME, HOMEPAGE);
	}
	
	/* initialize GConf client */
	client = gconf_client_get_default();
	gconf_client_add_dir(client, PATH, GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_add_dir(client, "/system/http_proxy", GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_add_dir(client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_NONE, NULL);

	gconf_client_notify_add(client, "/system/http_proxy", conf_proxy_reset_settings_cb, NULL, NULL, NULL);
	gconf_client_notify_add(client, "/desktop/gnome/interface/toolbar_style", conf_toolbar_style_settings_cb, NULL, NULL, NULL);
	gconf_client_notify_add(client, SHOW_TRAY_ICON, conf_tray_settings_cb, NULL, NULL, NULL);
	
	/* Load settings into static buffers */
	conf_proxy_reset_settings_cb(NULL, 0, NULL, NULL);

	if(0 == (NET_TIMEOUT = getNumericConfValue(NETWORK_TIMEOUT)))
		NET_TIMEOUT = 30;	/* default network timeout 30s */
}

/* maybe called several times to reload configuration */
void conf_load() {
	gint	maxitemcount;
	gchar *downloadPath;
	
	/* check if important preferences exist... */
	if(0 == (maxitemcount = getNumericConfValue(DEFAULT_MAX_ITEMS)))
		setNumericConfValue(DEFAULT_MAX_ITEMS, 100);
	
	downloadPath = getStringConfValue(ENCLOSURE_DOWNLOAD_PATH);
	if(0 == strcmp("", downloadPath))
		setStringConfValue(ENCLOSURE_DOWNLOAD_PATH, g_getenv("HOME"));
	g_free(downloadPath);
}

static void conf_tray_settings_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data) {
	GConfValue *value;
	if (entry != NULL) {
		value = gconf_entry_get_value(entry);
		if (value != NULL && value->type == GCONF_VALUE_BOOL)
			ui_tray_enable(gconf_value_get_bool(value));
	}
}

static void conf_toolbar_style_settings_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data) {
	GConfValue *value;
	if (entry != NULL) {
		
		value = gconf_entry_get_value(entry);
		if (value != NULL && value->type == GCONF_VALUE_STRING)
			ui_mainwindow_set_toolbar_style(gconf_value_get_string(value));
	}
}

static void conf_proxy_reset_settings_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data) {
	gchar		*tmp;
	gboolean	useGnomeProxy;
	xmlURIPtr uri;
	
	g_free(proxyname);
	proxyname = NULL;
	proxyport = 0;	
	
	g_free(proxyusername);
	proxyusername = NULL;
	g_free(proxypassword);
	proxypassword = NULL;
	
	/* check for a configured GNOME proxy, note: older
	   GNOME versions do use the boolean flag GNOME_USE_PROXY
	   while newer ones use the string key GNOME_PROXY_MODE */
	tmp = getStringConfValue(GNOME_PROXY_MODE);
	useGnomeProxy = getBooleanConfValue(GNOME_USE_PROXY) || g_str_equal(tmp, "manual");
	g_free(tmp);

	if(useGnomeProxy) {
		proxyname = getStringConfValue(PROXY_HOST);
		proxyport = getNumericConfValue(PROXY_PORT);
		debug2(DEBUG_CONF, "using GNOME configured proxy: \"%s\" port \"%d\"", proxyname, proxyport);
		if (getBooleanConfValue(PROXY_USEAUTH)) {
			proxyusername = getStringConfValue(PROXY_USER);
			proxypassword = getStringConfValue(PROXY_PASSWD);
		}
	} else {
		/* otherwise there could be a proxy specified in the environment 
		   the following code was derived from SnowNews' setup.c */
		if(g_getenv("http_proxy") != NULL) {
			/* The pointer returned by getenv must not be altered.
			   What about mentioning this in the manpage of getenv? */
			debug0(DEBUG_CONF, "using proxy from environment");
			do {
				uri = xmlParseURI(BAD_CAST g_getenv("http_proxy"));
				if (uri == NULL)
					break;
				if (uri->server == NULL) {
					xmlFreeURI(uri);
					break;
				}
				proxyname = g_strdup(uri->server);
				proxyport = (uri->port == 0) ? 3128 : uri->port;
				if (uri->user != NULL) {
					tmp = strtok(uri->user, ":");
					tmp = strtok(NULL, ":");
					if (tmp != NULL) {
						proxyusername = g_strdup(uri->user);
						proxypassword = g_strdup(tmp);
					}
				}
				xmlFreeURI(uri);
			} while (FALSE);
		}
	}
	
	ui_htmlview_set_proxy(proxyname, proxyport, proxyusername, proxypassword);
	debug4(DEBUG_CONF, "Proxy settings are now %s:%d %s:%s", proxyname != NULL ? proxyname : "NULL", proxyport,
		  proxyusername != NULL ? proxyusername : "NULL",
		  proxypassword != NULL ? proxypassword : "NULL");
}


/*----------------------------------------------------------------------*/
/* generic configuration access methods					*/
/*----------------------------------------------------------------------*/

void setBooleanConfValue(gchar *valuename, gboolean value) {
	GError		*err = NULL;
	GConfValue	*gcv;
	
	g_assert(valuename != NULL);
	
	gcv = gconf_value_new(GCONF_VALUE_BOOL);
	gconf_value_set_bool(gcv, value);
	gconf_client_set(client, valuename, gcv, &err);
	gconf_value_free(gcv);
	is_gconf_error(&err);
}

gboolean getBooleanConfValue(gchar *valuename) {
	GConfValue	*value = NULL;
	gboolean	result;

	g_assert(valuename != NULL);

	value = gconf_client_get(client, valuename, NULL);
	if(NULL == value) {
		setBooleanConfValue(valuename, FALSE);
		result = FALSE;
	} else {
		result = gconf_value_get_bool(value);
		gconf_value_free(value);
	}
		
	return result;
}

void setStringConfValue(gchar *valuename, const gchar *value) {
	GError		*err = NULL;
	GConfValue	*gcv;
	
	g_assert(valuename != NULL);
	
	gcv = gconf_value_new(GCONF_VALUE_STRING);
	gconf_value_set_string(gcv, value);
	gconf_client_set(client, valuename, gcv, &err);
	gconf_value_free(gcv);
	is_gconf_error(&err);
}

gchar * getStringConfValue(gchar *valuename) {
	GConfValue	*value = NULL;
	gchar		*result;

	g_assert(valuename != NULL);
		
	value = gconf_client_get(client, valuename, NULL);
	if(NULL == value) {
		result = g_strdup("");
	} else {
		result = (gchar *)g_strdup(gconf_value_get_string(value));
		gconf_value_free(value);
	}
		
	return result;
}

void setNumericConfValue(gchar *valuename, gint value) {
	GError		*err = NULL;
	GConfValue	*gcv;
	
	g_assert(valuename != NULL);
	debug2(DEBUG_CONF, "Setting %s to %d", valuename, value);
	gcv = gconf_value_new(GCONF_VALUE_INT);
	gconf_value_set_int(gcv, value);
	gconf_client_set(client, valuename, gcv, &err);
	is_gconf_error(&err);
	gconf_value_free(gcv);
}

gint getNumericConfValue(gchar *valuename) {
	GConfValue	*value;
	gint		result = 0;

	g_assert(valuename != NULL);
		
	value = gconf_client_get(client, valuename, NULL);
	if(NULL != value) {
		result = gconf_value_get_int(value);
		gconf_value_free(value);
	}
			
	return result;
}
