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

static gboolean is_gconf_error(GError **err) {

	if(*err != NULL) {
		g_print("%s\n", (*err)->message);
		g_error_free(*err);
		*err = NULL;
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
	int i;
	gchar *id = g_malloc(10);
	for(i=0;i<7;i++)
		id[i] = (char)g_random_int_range('a', 'z');
	id[7] = '\0';
	return id;
}

/*----------------------------------------------------------------------*/
/* config loading on startup						*/
/*----------------------------------------------------------------------*/

static void load_folder_contents(folderPtr folder, gchar* path);

static gboolean load_key(folderPtr parent, gchar *id) {
	int type, interval;
	gchar *path2, *name, *url, *cacheid;
	folderPtr folder;
	gboolean expanded;

	/* Type */
	path2 = build_path_str(id, "type");

	type = getNumericConfValue(path2);
	g_free(path2);
	
	if (type == 0)
		return FALSE;

	switch(type) {
	case FST_FOLDER:
		path2 = build_path_str(id, "feedlistname");
		name = getStringConfValue(path2);
		g_free(path2);

		path2 = build_path_str(id, "collapseState");
		expanded = !getBooleanConfValue(path2);
		g_free(path2);
		
		folder = restore_folder(parent, -1, name, id, FST_FOLDER);
		
		ui_add_folder(folder);
		load_folder_contents(folder, id);
		if (expanded)
			ui_folder_set_expansion(folder, TRUE);
		g_free(name);
		break;
	case FST_HELPFOLDER:
		folder = feedlist_insert_help_folder(parent);

		path2 = build_path_str(id, "collapseState");
		expanded = !getBooleanConfValue(path2);
		if (expanded)
			ui_folder_set_expansion(folder, TRUE);
		g_free(path2);
		break;
	default:
		path2 = build_path_str(id, "name");
		name = getStringConfValue(path2);
		g_free(path2);

		path2 = build_path_str(id, "updateInterval");
		interval = getNumericConfValue(path2);
		g_free(path2);	
		
		path2 = build_path_str(id, "url");
		url = getStringConfValue(path2);	/* we use this function to get a "" on empty conf value */
		g_free(path2);
		
		if(0 == type)
			type = FST_FOLDER;
			
		if (strchr(id,'/')) {
			cacheid = g_strdup(id);
			*(strchr(cacheid,'/')) = '_';
		} else {
			cacheid = g_strdup_printf("_%s",id);
		}

		feed_add(type, url, parent, name, cacheid, interval, FALSE);

		g_free(cacheid);
		g_free(url);
		g_free(name);
	}
	return TRUE;
}

static void load_folder_contents(folderPtr folder, gchar* fid) {
	GSList *list;
	gchar *id;
	GError		*err = NULL;
	gchar *name;
	
	/* First, try to look and (and migrate groups) */
	
	name = build_path_str(fid,"groups");
	
	list = gconf_client_get_list(client, name, GCONF_VALUE_STRING, &err);
	if (!is_gconf_error(&err) && list) {
		while (list != NULL) {
			id = (gchar*)list->data;
			g_assert(id);
			g_assert(NULL != id);
			load_key(folder, id);
			list = list->next;
		}
	}
	g_free(name);
	/* Then, look at the feedlist */
	name = build_path_str(fid, "feedlist");
	list = gconf_client_get_list(client, name, GCONF_VALUE_STRING, &err);

	if (!is_gconf_error(&err) && list) {
		while (list != NULL) {
			id = (gchar*)list->data;
			g_assert(id);
			g_assert(NULL != id);
			load_key(folder, id);
			list = list->next;
		}
	}
	g_free(name);
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

static gboolean is_number(gchar *s) {
	while (*s != '\0') {
		if(!g_ascii_isdigit(*s))
			return FALSE;
		s++;
	}
	return TRUE;
}

static void conf_feedlist_erase_gconf() {
	GSList *list, *iter;
	GError		*err = NULL;

	iter = list = gconf_client_all_dirs(client, PATH, &err);
	err=NULL;

	/* Remove all directories */
	while(!is_gconf_error(&err) && iter != NULL) {
		if (strstr(iter->data,"dir") != NULL || is_number(iter->data)) {
			g_message("Deleting %s", (gchar*)iter->data);
			gconf_client_recursive_unset(client, (gchar*)iter->data, GCONF_UNSET_INCLUDING_SCHEMA_NAMES, &err);
		}
		g_free(iter->data);
		iter = iter->next;
	}
	g_slist_free(list);

	gconf_client_unset(client, PATH "/groups", &err);
	is_gconf_error(&err);

	gconf_client_unset(client, PATH "/feedlist", &err);
	is_gconf_error(&err);
}

void loadSubscriptions(void) {
	gchar	*filename;
	
	feedlistLoading = TRUE;
	load_folder_contents(folder_get_root(), "");
	filename = g_strdup_printf("%s/.liferea/feedlist.opml", g_get_home_dir());
	importOPMLFeedList(filename, folder_get_root(), FALSE);
	g_free(filename);
	g_message("Erasing old gconf enteries.");
	conf_feedlist_erase_gconf();
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
	is_gconf_error(&err);
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
	is_gconf_error(&err);
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
	is_gconf_error(&err);
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
