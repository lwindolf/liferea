/*
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <libxml/nanohttp.h>
#include <string.h>
#include "support.h"
#include "callbacks.h"
#include "update.h"
#include "feed.h"
#include "folder.h"
#include "common.h"
#include "conf.h"

#define MAX_GCONF_PATHLEN	256

#define PATH		"/apps/liferea"

/* _() for HELP1URL to allow localised help feeds */
#define HELP1URL 	_("http://liferea.sf.net/help048.rdf")
#define HELP2URL	"http://sourceforge.net/export/rss2_projnews.php?group_id=87005&rss_fulltext=1"
#define HOMEPAGE	"http://liferea.sf.net/"

static GConfClient	*client;

static guint feedlist_save_timer;
static guint feedlistLoading;

/* configuration strings for the SnowNews HTTP code used from within netio.c */
char 	*useragent = NULL;
char	*proxyname = NULL;
int	proxyport = 0;

/* Function prototypes */

static gchar * build_path_str(gchar *str1, gchar *str2) {
	gchar	*gconfpath;

	g_assert(NULL != str1);
	if(0 == strcmp(str1, "")) 
		gconfpath = g_strdup_printf("%s/%s", PATH, str2);
	else
		gconfpath = g_strdup_printf("%s/%s/%s", PATH, str1, str2);
		
	return gconfpath;
}

static gboolean is_gconf_error(GError *err) {

	if(err != NULL) {
		g_print(err->message);
		g_error_free(err);
		err = NULL;
		return TRUE;
	}
	
	return FALSE;
}

/* called once on startup */
void initConfig() {
	int	ualength;
	char	*lang;
	
	/* have to be called for multithreaded programs */
	xmlInitParser();
	
	/* the following code was copied from SnowNews and adapted to build
	   a Liferea user agent... */
	
	/* Constuct the User-Agent string of Liferea. This is done here in program init,
	   because we need to do it exactly once and it will never change while the program
	   is running. */
	if (getenv("LANG") != NULL) {
		lang = getenv("LANG");
		/* e.g. Liferea/0.3.8 (Linux; de_DE; (http://liferea.sf.net/) */
		ualength = strlen("Liferea/") + strlen(VERSION) + 2 + strlen(lang) + 2 + strlen(OSNAME)+2 + strlen(HOMEPAGE) + 2;
		useragent = g_malloc(ualength);
		snprintf (useragent, ualength, "Liferea/%s (%s; %s; %s)", VERSION, OSNAME, lang, HOMEPAGE);
	} else {
		/* "Liferea/" + VERSION + "(" OS + "; " + HOMEPAGE + ")" */
		ualength = strlen("Liferea/") + strlen(VERSION) + 2 + strlen(OSNAME) + 2 + strlen("http://liferea.sf.net/") + 2;
		useragent = g_malloc(ualength);
		snprintf (useragent, ualength, "Liferea/%s (%s; %s)", VERSION, OSNAME, HOMEPAGE);
		printf ("%s\n", useragent);
	}
	
	/* initialize GConf client */
	if (client == NULL) {
		client = gconf_client_get_default();
		gconf_client_add_dir(client, PATH, GCONF_CLIENT_PRELOAD_NONE, NULL);
	}
}

/* maybe called several times to reload configuration */
void loadConfig() {
	gint	maxitemcount;

	/* check if important preferences exist... */
	if(0 == (maxitemcount = getNumericConfValue(DEFAULT_MAX_ITEMS)))
		setNumericConfValue(DEFAULT_MAX_ITEMS, 100);

	if(getBooleanConfValue(USE_PROXY)) {
		g_free(proxyname);
	        proxyname = getStringConfValue(PROXY_HOST);
        	proxyport = getNumericConfValue(PROXY_PORT);
	} else {
		g_free(proxyname);
		proxyname = NULL;
		proxyport = 0;
	}
}

/*----------------------------------------------------------------------*/
/* feed entry handling							*/
/*----------------------------------------------------------------------*/

gchar* conf_new_id() {
	GRand *grand = g_rand_new();
	int i;
	gchar *id = g_malloc(10);
	for(i=0;i<7;i++)
		id[i] = (char)g_rand_int_range(grand, 'a', 'z');
	id[7] = '\0';
	g_rand_free(grand);
	return id;
}

/*----------------------------------------------------------------------*/
/* config loading on startup						*/
/*----------------------------------------------------------------------*/

static void load_folder_contents(folderPtr folder, gchar* path, folderPtr *helpFolder);

static gboolean load_key(folderPtr parent, gchar *prefix, gchar *id, folderPtr *helpFolder) {
	GError		*err = NULL;
	int type, interval;
	gchar *path2, *name, *url;
	folderPtr folder;
	gboolean expanded;

	/* Type */
	path2 = g_strdup_printf("%s/%s/type", prefix, id);
	type = gconf_client_get_int(client, path2, &err);
	
	if (type == 0 || is_gconf_error(err))
		return FALSE;
	g_free(path2);

	switch(type) {
	case FST_FOLDER:
		path2 = g_strdup_printf("%s/%s/feedlistname", prefix, id);
		name = gconf_client_get_string(client, path2, &err);
		g_message("Loading feed with title %s at %s", name, path2);
		is_gconf_error(err);
		g_free(path2);

		path2 = g_strdup_printf("%s/%s/collapseState", prefix, id);
		expanded = !getBooleanConfValue(path2);
		g_message("%s is expanded? %d", path2, expanded);
		g_free(path2);

		path2 = g_strdup_printf("%s/%s", prefix, id);

		folder = restore_folder(parent, -1, name, id, FST_FOLDER);
		
		ui_add_folder(folder);
		load_folder_contents(folder, path2, helpFolder);
		if (expanded)
			ui_folder_set_expansion(folder, TRUE);
		g_free(path2);
		break;
	case FST_HELPFOLDER:
		*helpFolder = restore_folder(parent, -1, _("Liferea Help"), id, FST_HELPFOLDER);
		ui_add_folder(*helpFolder);
		feed_add(FST_HELPFEED, HELP1URL, *helpFolder, _("Online Help Feed"), NULL, 1440, FALSE) ;
		feed_add(FST_HELPFEED, HELP2URL, *helpFolder, _("Liferea SF News"), NULL, 1440, FALSE) ;

		path2 = g_strdup_printf("%s/%s/collapseState", prefix, id);
		expanded = !getBooleanConfValue(path2);
		if (expanded)
			ui_folder_set_expansion(*helpFolder, TRUE);
		g_free(path2);
		break;
	default:
		path2 = g_strdup_printf("%s/%s/name", prefix, id);
		g_message("%s", path2);
		name = gconf_client_get_string(client, path2, &err);
		is_gconf_error(err);
		g_free(path2);

		path2 = g_strdup_printf("%s/%s/updateInterval", prefix, id);
		g_message("%s", path2);
		interval = gconf_client_get_int(client, path2, &err);
		is_gconf_error(err);
		g_free(path2);	
		
		path2 = g_strdup_printf("%s/%s/url", prefix, id);
		g_message("%s", path2);
		url = getStringConfValue(path2);	/* we use this function to get a "" on empty conf value */
		g_free(path2);
		
		if(0 == type)
			type = FST_FOLDER;
			
		if(0 == interval)
			interval = -1;
		
		feed_add(type, url, parent, name, id, interval, FALSE);
		g_free(id);
		g_free(url);
	}
	g_free(name);
	return TRUE;
}

static void load_folder_contents(folderPtr folder, gchar* path, folderPtr *helpFolder) {
	GSList *list;
	gchar *id;
	GError		*err = NULL;
	gchar *name;
	
	g_message("Loading from gconf: %s, path: %s", folder->title, path);

	/* First, try to look and (and migrate groups) */

	name = g_strdup_printf("%s/groups", path);

	list = gconf_client_get_list(client, name, GCONF_VALUE_STRING, &err);
	is_gconf_error(err);

	if (list) {
		while (list != NULL) {
			id = (gchar*)list->data;
			g_assert(id);
			g_message("**found value of %s", id);
			g_assert(NULL != id);
			load_key(folder, path, id, helpFolder);
			list = list->next;
		}
	}
	g_free(name);
	/* Then, look at the feedlist */
	name = g_strdup_printf("%s/feedlist", path);
	list = gconf_client_get_list(client, name, GCONF_VALUE_STRING, &err);
	is_gconf_error(err);

	if (list) {
		while (list != NULL) {
			id = (gchar*)list->data;
			g_assert(id);
			g_message("**found value of %s", id);
			g_assert(NULL != id);
			load_key(folder, path, id, helpFolder);
			list = list->next;
		}
	}
	g_free(name);
	/*
	nodes = gconf_value_new(GCONF_VALUE_LIST);
	gconf_value_set_list_type(nodes, GCONF_VALUE_STRING);
	gconf_value_set_list(nodes, (GSList*)NULL);
	gconf_client_set(client, path2, nodes, &err);
	is_gconf_error(err);
	gconf_value_free(nodes);
	g_free(path2);
	*/
	g_message("done reading %s", path);
}

folderPtr feedlist_insert_help_folder(folderPtr parent) {
	static folderPtr helpFolder = NULL;

	if (helpFolder == NULL) {
		helpFolder = restore_folder(parent, -1, _("Liferea Help"), "helpFolder", FST_HELPFOLDER);
		ui_add_folder(helpFolder);
		feed_add(FST_HELPFEED, HELP1URL, helpFolder, _("Online Help Feed"), "helpfeed1", 1440, FALSE) ;
		feed_add(FST_HELPFEED, HELP2URL, helpFolder, _("Liferea SF News"), "helpfeed2", 1440, FALSE) ;
	}
	return helpFolder;
}

void loadSubscriptions(void) {
	gchar *filename;
	//load_folder_contents(rootFolder, PATH);
	feedlistLoading = TRUE;
	filename = g_strdup_printf("%s/.liferea/feedlist.opml", g_get_home_dir());
	importOPMLFeedList(filename, folder_get_root());
	g_free(filename);

	/* if help folder was not yet created... */
	feedlist_insert_help_folder(folder_get_root());
	feedlistLoading = FALSE;
}

void conf_feedlist_save() {
	gchar *filename, *filename_real;
	
	if(feedlistLoading)
		return;

	g_message(_("Saving feedlist"));
	filename = g_strdup_printf("%s/feedlist.opml~", getCachePath());

	if (0 == exportOPMLFeedList(filename)) {
		filename_real = g_strdup_printf("%s/feedlist.opml", getCachePath());
		if(rename(filename, filename_real) < 0)
			g_warning(_("Error renaming %s to %s\n"), filename, filename_real);
	}
}

static gboolean conf_feedlist_schedule_save_cb(gpointer user_data) {
	conf_feedlist_save();
	feedlist_save_timer = 0;
	return FALSE;
}

void conf_feedlist_schedule_save() {
	if (!feedlistLoading && !feedlist_save_timer) {
		g_message(_("Scheduling feedlist save"));
		feedlist_save_timer = g_timeout_add(5000, conf_feedlist_schedule_save_cb, NULL);
	}
}
/* returns true if namespace is enabled in configuration */
gboolean getNameSpaceStatus(gchar *nsname) {
	GConfValue	*value = NULL;
	gchar		*gconfpath;
	gboolean	status;
	
	g_assert(NULL != nsname);
	gconfpath = g_strdup_printf("%s/ns_%s", PATH, nsname);
	value = gconf_client_get_without_default(client, gconfpath, NULL);
	if(NULL == value) {
		g_print(_("RSS namespace %s not yet configured! Activating...\n"), nsname);
		setNameSpaceStatus(nsname, TRUE);
		status = TRUE;
	} else {
		status = gconf_value_get_bool(value);
	}
	g_free(gconfpath);
	g_free(value);	
	return status;
}

/* used to enable/disable a namespace in configuration */
void setNameSpaceStatus(gchar *nsname, gboolean enable) {
	gchar		*gconfpath;
	
	g_assert(NULL != nsname);
		
	gconfpath = g_strdup_printf("%s/ns_%s", PATH, nsname);
	setBooleanConfValue(gconfpath, enable);
	g_free(gconfpath);
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
	is_gconf_error(err);
}

gboolean getBooleanConfValue(gchar *valuename) {
	GConfValue	*value = NULL;
	gboolean	result;

	g_assert(valuename != NULL);

	value = gconf_client_get_without_default(client, valuename, NULL);
	if(NULL == value) {
		setBooleanConfValue(valuename, FALSE);
		result = FALSE;
	} else {
		result = gconf_value_get_bool(value);
		gconf_value_free(value);
	}
		
	return result;
}

void setStringConfValue(gchar *valuename, gchar *value) {
	GError		*err = NULL;
	GConfValue	*gcv;
	
	g_assert(valuename != NULL);
	
	gcv = gconf_value_new(GCONF_VALUE_STRING);
	gconf_value_set_string(gcv, value);
	gconf_client_set(client, valuename, gcv, &err);
	gconf_value_free(gcv);
	is_gconf_error(err);
}

gchar * getStringConfValue(gchar *valuename) {
	GConfValue	*value = NULL;
	gchar		*result;

	g_assert(valuename != NULL);
		
	value = gconf_client_get_without_default(client, valuename, NULL);
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
	g_message("Setting %s to %d", valuename, value);
	gcv = gconf_value_new(GCONF_VALUE_INT);
	gconf_value_set_int(gcv, value);
	gconf_client_set(client, valuename, gcv, &err);
	is_gconf_error(err);
	gconf_value_free(gcv);
}

gint getNumericConfValue(gchar *valuename) {
	GConfValue	*value;
	gint		result = 0;

	g_assert(valuename != NULL);
		
	value = gconf_client_get_without_default(client, valuename, NULL);
	if(NULL != value) {
		result = gconf_value_get_int(value);
		gconf_value_free(value);
	}
			
	return result;
}
