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
#include "feed.h"
#include "common.h"
#include "conf.h"

#define MAX_GCONF_PATHLEN	256

#define PATH		"/apps/liferea"
#define GROUPS		"/apps/liferea/groups"

/* _() for HELP1URL to allow localised help feeds */
#define HELP1URL 	_("http://liferea.sf.net/help046.rdf")
#define HELP2URL	"http://sourceforge.net/export/rss2_projnews.php?group_id=87005&rss_fulltext=1"
#define HOMEPAGE	"http://liferea.sf.net/"

static GConfClient	*client;

/* configuration strings for the SnowNews HTTP code used from within netio.c */
char 	*useragent = NULL;
char	*proxyname = NULL;
int	proxyport = 0;

static gchar * build_path_str(gchar *str1, gchar *str2) {
	gchar	*gconfpath;

	g_assert(NULL != str1);
	if(0 == strcmp(str1, "")) 
		gconfpath = g_strdup_printf("%s/%s", PATH, str2);
	else
		gconfpath = g_strdup_printf("%s/%s/%s", PATH, str1, str2);
		
	return gconfpath;
}

free_gconf_key(gchar *key, gchar *attribute) {
	gchar	*gconfpath;
	
	gconfpath = build_path_str(key, attribute);
	gconf_client_unset(client, gconfpath, NULL);
	g_free(gconfpath);
}

gboolean is_gconf_error(GError *err) {

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
	   
        /* Constuct the User-Agent string of snownews. This is done here in program init,
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
}

/* maybe called several times to reload configuration */
void loadConfig() {
	GError	*err = NULL;
	gchar	*proxy_url;
	gint	maxitemcount;
	
	/* GConf client */
	if (client == NULL)
		client = gconf_client_get_default();

	gconf_client_add_dir(client, PATH, GCONF_CLIENT_PRELOAD_NONE, NULL);	

	/* check if several preferences exist */
	if(0 == (maxitemcount = getNumericConfValue(DEFAULT_MAX_ITEMS)))
		setNumericConfValue(DEFAULT_MAX_ITEMS, 100);

        /* load proxy settings for libxml */
        xmlNanoHTTPInit();
        
	if(getBooleanConfValue(USE_PROXY)) {
	        proxyname = getStringConfValue(PROXY_HOST);
        	proxyport = getNumericConfValue(PROXY_PORT);

        	proxy_url = g_strdup_printf("http://%s:%d/", proxyname, proxyport);
		g_print("using proxy: \"%s\"\n", proxy_url);
		// FIXME: don't use NanoHTTP anymore!!!	
        	xmlNanoHTTPScanProxy(proxy_url);
		g_free(proxy_url);
	} else {
		if(NULL != proxyname) {
			g_free(proxyname);
			proxyname = NULL;
		}
		
		/* this is neccessary to reset proxy after config change */
		// FIXME: don't use NanoHTTP anymore!!!	
        	xmlNanoHTTPScanProxy(NULL);	
	}
}

/* return the entry list of a given key prefix */
GSList * getFeedKeyList(gchar *keyprefix) {
	GSList		*list = NULL;
	GError		*err = NULL;
	GConfValue	*value;
	gchar		*gconfpath;

	gconfpath = build_path_str(keyprefix, "feedlist");
	
	if(NULL != (value = gconf_client_get(client, gconfpath, &err))) {
		is_gconf_error(err);
		list = gconf_value_get_list(value);
		g_free(value);
	} else {
		g_warning(g_strdup_printf(_("internal error! could not retrieve feed list for gconf path %s\n"), gconfpath));
	}
	g_free(gconfpath);
	
	return list;
}

void setFeedKeyList(gchar *keyprefix, GSList *newlist) {
	GConfValue	*value;
	GError		*err = NULL;
	gchar		*gconfpath;	

	gconfpath = build_path_str(keyprefix, "feedlist");

	value = gconf_value_new(GCONF_VALUE_LIST);
	gconf_value_set_list_type(value, GCONF_VALUE_STRING);
	gconf_value_set_list(value, newlist);
	gconf_client_set(client, gconfpath, value, &err);
	is_gconf_error(err);
	g_free(value);
	g_free(gconfpath);
}

void removeFeedFromConfig(gchar *keyprefix, gchar *key) {
	GError		*err = NULL;
	GConfValue	*element;
	GSList		*list, *newlist = NULL;
	GSList		*iter;
	gchar		*gconfpath;
	const char	*tmp;
	int 		found = 0, error = 0;

	/* remove key from key list */
	iter = list = getFeedKeyList(keyprefix);
	while(iter != NULL) {
		element = iter->data;
		tmp = gconf_value_get_string(element);
		if(0 != strcmp(tmp, key)) {
			newlist = g_slist_append(newlist, element);
		} else {
			found = 1;
		}
		iter = g_slist_next(iter);		
	}
	
	if(!found)
		g_warning("internal error: could not find key in configuration!");
	
	/* write new list back to gconf */
	setFeedKeyList(keyprefix, newlist);
	
	/* remove entry gconf keys */
	free_gconf_key(key, "url");
	free_gconf_key(key, "name");
	free_gconf_key(key, "type");
	free_gconf_key(key, "updateInterval");
	
	g_slist_free(list);
	g_slist_free(newlist);
}

/* compare function for numerical GSList sorting in getFreeFeedKey() */
static gint compare_func(gconstpointer a, gconstpointer b) {
	GError		*err = NULL;
	GConfValue	*element;
	const char	*feedkey1, *feedkey2;
	char		*tmp, *tmp1, *tmp2;
	gint		result, res;
	
	element = (GConfValue *)a;
	feedkey1 = gconf_value_get_string(element);
	is_gconf_error(err);
	
	element = (GConfValue *)b;
	feedkey2 = gconf_value_get_string(element);
	is_gconf_error(err);	
	
	/* skip the keyprefixes to get the key numbers */
	if(NULL == (tmp1 = strrchr(feedkey1, '/'))) {
		tmp1 = (char *)feedkey1;
	} else {
		tmp1++;
	}	
	/* skip chars in front of directory keys */
	if(NULL != (tmp = strpbrk(tmp1, "0123456789")))
		tmp1 = tmp;
	
	if(NULL == (tmp2 = strrchr(feedkey2, '/'))) {
		tmp2 = (char *)feedkey2;
	} else {
		tmp2++;
	}
	/* skip chars in front of directory keys */
	if(NULL != (tmp = strpbrk(tmp2, "0123456789")))
		tmp2 = tmp;

	result = atoi(tmp1) - atoi(tmp2);
	res = (result)?result/abs(result):0;

	return res;
}

/* this method is used to get a feedkey which is not already
   used for an existing feed */
gchar * getFreeFeedKey(gchar *keyprefix) {
	GError		*err = NULL;
	GSList		*iter = NULL;	
	GConfValue	*element;
	gchar		*gconfpath;
	gchar		*url;
	const char 	*key;
	int 		i;

	/* actually we use only numbers as feedkeys... */
	int		nr = 1;
	gchar		*newkey;	/* the number as a string */

	g_assert(keyprefix != NULL);	
	if(0 == strcmp(keyprefix, ""))
		newkey = g_strdup_printf("%d", nr);
	else
		newkey = g_strdup_printf("%s/%d", keyprefix, nr);
				
	iter = getFeedKeyList(keyprefix);
	iter = g_slist_sort(iter, compare_func);	
	while (iter != NULL) {
		element = iter->data;
		key = gconf_value_get_string(element);
		is_gconf_error(err);	

		g_assert(NULL != key);
		if(0 == strcmp(key, newkey)) {
			nr++;
			g_free(newkey);
			
			if(0 == strcmp(keyprefix, ""))
				newkey = g_strdup_printf("%d", nr);
			else
				newkey = g_strdup_printf("%s/%d", keyprefix, nr);
		}
	
		iter = g_slist_next(iter);
	}

	return newkey;
}

/*----------------------------------------------------------------------*/
/* folder entry handling							*/
/*----------------------------------------------------------------------*/

int setFolderTitleInConfig(gchar *keyprefix, gchar *title) {
	gchar 	*gconfpath;
	
	gconfpath = g_strdup_printf("%s/%s/feedlistname", PATH, keyprefix);
	setStringConfValue(gconfpath, title);
	g_free(gconfpath);

	return 0;	
}

int setFolderTypeInConfig(gchar *keyprefix, gint type) {
	gchar 	*gconfpath;
	
	gconfpath = g_strdup_printf("%s/%s/type", PATH, keyprefix);
	setNumericConfValue(gconfpath, type);
	g_free(gconfpath);

	return 0;	
}

/* adds the given keyprefix to to the list of groups
   and saves the title in the directory path as key 
   feedlisttitle, on successful execution this function
   returns the new directory key... */
gchar * addFolderToConfig(gchar *title) {

	GError		*err = NULL;
	GSList		*list, *newlist, *iter;
	int 		error = 0;
	GConfValue	*value, *value2;
	GConfValue	*element;
	gchar		*gconfpath;
	gchar		*url, *newdirkey;
	const char 	*dirkey;
	int 		i;

	/* first step: find free directory key */

	/* we use "dir#" were # is a number as directory keys */
	int		nr = 1;
	gchar		*newdir;

	newdirkey = g_strdup_printf("dir%d", nr);
				
	gconfpath = g_strdup_printf("%s/groups", PATH);
	value = gconf_client_get(client, gconfpath, &err);
	is_gconf_error(err);

	list = gconf_value_get_list(value);
	g_free(value);
	g_free(gconfpath);
	
	iter = g_slist_sort(list, compare_func);	
	while (iter != NULL) {
		element = iter->data;
		dirkey = gconf_value_get_string(element);
		is_gconf_error(err);	

		g_assert(NULL != dirkey);
		if(0 == strcmp(dirkey, newdirkey)) {
			nr++;
			g_free(newdirkey);			
			newdirkey = g_strdup_printf("dir%d", nr);		
		}
	
		iter = g_slist_next(iter);
	}

	/* second step: insert key into directory list */
	value = gconf_value_new(GCONF_VALUE_STRING);
	gconf_value_set_string(value, newdirkey);
	is_gconf_error(err);
	newlist = g_slist_append(list, value);

	/* write new list back to gconf */
	gconfpath = g_strdup_printf("%s/groups", PATH);

	value2 = gconf_value_new(GCONF_VALUE_LIST);
	gconf_value_set_list_type(value2, GCONF_VALUE_STRING);
	gconf_value_set_list(value2, newlist);
	gconf_client_set(client, gconfpath, value2, &err);
	is_gconf_error(err);
	g_free(value);
	g_free(value2);
	g_free(gconfpath);

	/* last step: save directory title & type*/	
	setFolderTitleInConfig(newdirkey, title);
	setFolderTypeInConfig(newdirkey, FST_NODE);
	
	g_slist_free(newlist);
	
	return newdirkey;
}

void removeFolderFromConfig(gchar *keyprefix) {
	GError		*err = NULL;
	GConfValue	*element, *value;
	GSList		*list, *newlist = NULL;
	GSList		*iter;
	const char	*tmpkeyprefix;
	gchar		*gconfpath;
	int 		error = 0;

	/* remove key from key list */
	gconfpath = g_strdup_printf("%s/groups", PATH);
	value = gconf_client_get(client, gconfpath, &err);
	is_gconf_error(err);

	iter = list = gconf_value_get_list(value);
	g_free(value);

	while(iter != NULL) {
		element = iter->data;
		tmpkeyprefix = gconf_value_get_string(element);
		if(0 != strcmp(tmpkeyprefix, keyprefix)) {
			newlist = g_slist_append(newlist, element);
		}
		iter = g_slist_next(iter);		
	}
	
	/* write new list back to gconf */
	value = gconf_value_new(GCONF_VALUE_LIST);
	gconf_value_set_list_type(value, GCONF_VALUE_STRING);
	gconf_value_set_list(value, newlist);
	gconf_client_set(client, gconfpath, value, &err);
	is_gconf_error(err);
	g_free(value);
	g_free(gconfpath);
	
	/* remove entry gconf keys */
	free_gconf_key(keyprefix, "feedlistname");
	free_gconf_key(keyprefix, "feedlist");
	free_gconf_key(keyprefix, "type");
	
	g_slist_free(list);
	g_slist_free(newlist);
}

/*----------------------------------------------------------------------*/
/* feed entry handling							*/
/*----------------------------------------------------------------------*/

/* adds the given URL to the list of feeds... */
gchar * addFeedToConfig(gchar *keyprefix, gchar *url, gint type) {
	GError		*err = NULL;
	GConfValue	*newkey = NULL;
	GSList		*list, *newlist;
	gchar		*key;
	int 		error = 0;

	/* get available key */
	if(NULL == (key = getFreeFeedKey(keyprefix))) {
		g_print(_("error! could not get a free entry key!\n"));
		return NULL;
	}

	/* save feed url and type */	
	if((NULL != url) && (0 != setFeedURLInConfig(key, url)))
		g_print(_("error! could not set a URL for this key!\n"));	

	if(0 != setFeedTypeInConfig(key, type)) 
		g_print(_("error! could not set a type for this key!\n"));
		
	/* add feedkey to feedlist */
	list = getFeedKeyList(keyprefix);
		
	newkey = gconf_value_new(GCONF_VALUE_STRING);
	gconf_value_set_string(newkey, key);
	is_gconf_error(err);
	
	newlist = g_slist_append(list, newkey);
	
	/* write new list back to gconf */
	setFeedKeyList(keyprefix, newlist);
		
	g_free(newkey);
	g_slist_free(newlist);
	
	return key;
}

int setFeedTitleInConfig(gchar *key, gchar *title) {
	GError		*err = NULL;
	gchar		*gconfpath;
					
	/* update title */
	gconfpath = g_strdup_printf("%s/%s/name", PATH, key);
	gconf_client_set_string(client, gconfpath, title, &err);
	g_free(gconfpath);	
	if(is_gconf_error(err))
		return 1;
	
	return 0;	
}

int setFeedTypeInConfig(gchar *key, gint type) {
	GError		*err = NULL;
	gchar		*gconfpath;
					
	/* update URL */
	gconfpath = g_strdup_printf("%s/%s/type", PATH, key);
	gconf_client_set_int(client, gconfpath, type, &err);
	g_free(gconfpath);
	if(is_gconf_error(err))
		return 1;
	
	return 0;	
}

int setFeedURLInConfig(gchar *key, gchar *url) {
	GError		*err = NULL;
	gchar		*gconfpath;
					
	/* update URL */
	gconfpath = g_strdup_printf("%s/%s/url", PATH, key);
	gconf_client_set_string(client, gconfpath, url, &err);
	g_free(gconfpath);	
	if(is_gconf_error(err))
		return 1;
	
	return 0;	
}

int setFeedUpdateIntervalInConfig(gchar *feedkey, gint interval) {
	GError		*err = NULL;
	gchar		*gconfpath;
					
	gconfpath = g_strdup_printf("%s/%s/updateInterval", PATH, feedkey);
	gconf_client_set_int(client, gconfpath, interval, &err);
	g_free(gconfpath);	
	if(is_gconf_error(err))
		return 1;
	
	return 0;
}

int setFolderCollapseStateInConfig(gchar *keyprefix, gboolean collapsed) {
	GError		*err = NULL;
	gchar		*gconfpath;
	
	gconfpath = g_strdup_printf("%s/%scollapseState", PATH, keyprefix);
	gconf_client_set_bool(client, gconfpath, collapsed, &err);
	g_free(gconfpath);	
	if(is_gconf_error(err))
		return 1;
	
	return 0;
}

/*----------------------------------------------------------------------*/
/* config loading on startup						*/
/*----------------------------------------------------------------------*/

void loadEntries() {
	GError		*err = NULL;
	GSList		*groupiter = NULL, *iter = NULL;
	GConfValue	*element, *keylist, *groups;
	gchar		*gconfpath;
	gchar		*filename, *name, *url, *keyprefix, *keylisttitle;
	const char	*key;
	gchar 		*helpFolderPrefix = NULL;
	gint		interval;
	gint		type;

	/* get (and create if not yet existing) entry group prefix list */
	groups = gconf_client_get(client, GROUPS, &err);
	is_gconf_error(err);

	if(NULL == groups) {
		/* first call key still does not exists, we create
		   an list with the standard key "" */
		element = gconf_value_new(GCONF_VALUE_STRING);		   
		gconf_value_set_string(element, g_strdup(""));
		groupiter = g_slist_append(groupiter, element);
		groups = gconf_value_new(GCONF_VALUE_LIST);
		gconf_value_set_list_type(groups, GCONF_VALUE_STRING);
		gconf_value_set_list(groups, groupiter);
		gconf_client_set(client, GROUPS, groups, &err);
		is_gconf_error(err);
	}

	if(GCONF_VALUE_LIST != groups->type) {
		g_print("group list: fatal thats not a list type!\n");
		return;
	}
	
	groupiter = gconf_value_get_list(groups);
	while (groupiter != NULL) {
		/* get keyprefix, build gconf path and get key list */
		element = groupiter->data;
		keyprefix = (gchar *)gconf_value_get_string(element);
		g_assert(NULL != keyprefix);
		
		gconfpath = build_path_str(keyprefix, "feedlist");		
		keylist = gconf_client_get(client, gconfpath, &err);
		is_gconf_error(err);
				
		if(NULL == keylist) {
			/* first call keylist still does not exists, we create
			   an empty list */
			keylist = gconf_value_new(GCONF_VALUE_LIST);
			gconf_value_set_list_type(keylist, GCONF_VALUE_STRING);
			gconf_value_set_list(keylist, NULL);
			gconf_client_set(client, gconfpath, keylist, &err);
			is_gconf_error(err);
		}
		g_free(gconfpath);
		
		gconfpath = build_path_str(keyprefix, "feedlistname");
		keylisttitle = getStringConfValue(gconfpath);		
		g_free(gconfpath);

		gconfpath = build_path_str(keyprefix, "type");
		type = getNumericConfValue(gconfpath);		
		g_free(gconfpath);

		if(0 == type)
			type = FST_NODE;
			
		if(FST_HELPNODE == type)
			helpFolderPrefix = keyprefix;
			
		addFolder(keyprefix, keylisttitle, type);
		/* don't free keyprefix because its used as hash table key in backend */
		g_free(keylisttitle);

		if(GCONF_VALUE_LIST != keylist->type) {
			g_print("fatal thats not a list type!\n");
			return;
		}
		
		iter = gconf_value_get_list(keylist);
		while (iter != NULL) {
		
			element = iter->data;
			key = gconf_value_get_string(element);
			is_gconf_error(err);		
			g_assert(NULL != key);			

			gconfpath = g_strdup_printf("%s/%s/type", PATH, key);
			type = gconf_client_get_int(client, gconfpath, &err);	/* retrieve the values manually to avoid error messages */
			is_gconf_error(err);
			g_free(gconfpath);

			gconfpath = g_strdup_printf("%s/%s/name", PATH, key);
			name = gconf_client_get_string(client, gconfpath, &err);
			is_gconf_error(err);
			g_free(gconfpath);	

			gconfpath = g_strdup_printf("%s/%s/updateInterval", PATH, key);
			interval = gconf_client_get_int(client, gconfpath, &err);
			is_gconf_error(err);
			g_free(gconfpath);	

			gconfpath = g_strdup_printf("%s/%s/url", PATH, key);
			url = getStringConfValue(gconfpath);	/* we use this function to get a "" on empty conf value */
			g_free(gconfpath);

			if(0 == type)
				type = FST_RSS;
				
			if(0 == interval)
				interval = -1;

			addFeed(type, url, (gchar *)key, keyprefix, name, interval);

			iter = g_slist_next(iter);
		}
			
		// FIXME: free keylist data with g_slist_free(...
		g_free(keylist);		
		groupiter = g_slist_next(groupiter);
		
		/* restore folder collapse state */
		gconfpath = g_strdup_printf("%s/%scollapseState", PATH, keyprefix);
		setFolderCollapseState(keyprefix, getBooleanConfValue(gconfpath));
		g_free(gconfpath);		
	}
	
	g_slist_free(groupiter);
	g_free(groups);
	
	/* if help folder was not yet created... */
	if(NULL == helpFolderPrefix) {
		/* add to config and UI feed list*/
		helpFolderPrefix = addFolderToConfig(_("Liferea Help"));
		setFolderTypeInConfig(helpFolderPrefix, FST_HELPNODE);
		addFolder(helpFolderPrefix, _("Liferea Help"), FST_HELPNODE );
	}
		
	g_assert(NULL != helpFolderPrefix);
	addFeed(FST_HELPFEED, HELP1URL, HELP1KEY, helpFolderPrefix, g_strdup(_("Online Help Feed")), 1440);
	addFeed(FST_HELPFEED, HELP2URL, HELP2KEY, helpFolderPrefix, g_strdup(_("Liferea SF News")), 1440);
	
	checkForEmptyFolders();
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
		result = (gchar *)gconf_value_get_string(value);
	}
		
	return result;
}

void setNumericConfValue(gchar *valuename, gint value) {
	GError		*err = NULL;
	GConfValue	*gcv;
	
	g_assert(valuename != NULL);
	
	gcv = gconf_value_new(GCONF_VALUE_INT);
	gconf_value_set_int(gcv, value);
	gconf_client_set(client, valuename, gcv, &err);
	is_gconf_error(err);
}

gint getNumericConfValue(gchar *valuename) {
	GConfValue	*value;
	gint		result = 0;

	g_assert(valuename != NULL);
		
	value = gconf_client_get_without_default(client, valuename, NULL);
	if(NULL != value) {
		result = gconf_value_get_int(value);
	}
			
	return result;
}
