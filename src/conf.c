/*
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <libxml/nanohttp.h>
#include "support.h"
#include "callbacks.h"
#include "conf.h"

#define MAX_GCONF_PATHLEN	256

#define PATH		"/apps/liferea"
#define GROUPS		"/apps/liferea/groups"

GConfClient	*client;

static gchar * build_path_str(gchar *str1, gchar *str2) {
	gchar	*gconfpath;

	g_assert(NULL != str1);
	if(0 == strcmp(str1, "")) 
		gconfpath = g_strdup_printf("%s/%s", PATH, str2);
	else
		gconfpath = g_strdup_printf("%s/%s/%s", PATH, str1, str2);
		
	return gconfpath;
}

int is_gconf_error(GError *err) {

	if(err != NULL) {
		g_print(err->message);
		g_error_free(err);
		err = NULL;
		return 1;
	}
	
	return 0;
}

void initConfig() {
	/* have to be called for multithreaded programs */
	xmlInitParser();
}

void loadConfig() {
	GError	*err = NULL;
        gchar	*proxy_url;
        gint	proxy_port;
        gchar	*proxy_host;
	
	/* GConf client */
	if (client == NULL)
		client = gconf_client_get_default();

	gconf_client_add_dir(client, PATH, GCONF_CLIENT_PRELOAD_NONE, NULL);	

	/* check if several preferences exist */

        /* load proxy settings for libxml */
        xmlNanoHTTPInit();
        
	if(getBooleanConfValue(USE_PROXY)) {
	        proxy_host = getStringConfValue(PROXY_HOST);
        	proxy_port = getNumericConfValue(PROXY_PORT);
		
        	proxy_url = g_strdup_printf("http://%s:%d/", proxy_host, proxy_port);
		g_print("using proxy: \"%s\"\n", proxy_url);
        	xmlNanoHTTPScanProxy(proxy_url);
		
		g_free(proxy_host);
		g_free(proxy_url);
	} else {
		/* this is neccessary to reset proxy after config change */
        	xmlNanoHTTPScanProxy(NULL);	
	}
}

/* return the entry list of a given type */
GSList * getEntryKeyList(gchar *keyprefix) {
	GSList		*list = NULL;
	GError		*err = NULL;
	GConfValue	*value;
	gchar		*gconfpath;

	gconfpath = build_path_str(keyprefix, "feedlist");
	
	value = gconf_client_get(client, gconfpath, &err);
	is_gconf_error(err);
	
	list = gconf_value_get_list(value);
	g_free(value);
	g_free(gconfpath);
	
	return list;
}

static void setEntryKeyList(gchar *keyprefix, GSList *newlist) {
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

void removeEntryFromConfig(gchar *keyprefix, gchar *key) {
	GError		*err = NULL;
	GConfValue	*element;
	GSList		*list, *newlist = NULL;
	GSList		*iter;
	const char	*tmp;
	int 		error = 0;

	/* remove key from key list */
	iter = list = getEntryKeyList(keyprefix);
	while(iter != NULL) {
		element = iter->data;
		tmp = gconf_value_get_string(element);
		if(0 != strcmp(tmp, key)) {
			newlist = g_slist_append(newlist, element);
		}
		iter = g_slist_next(iter);		
	}
	
	/* write new list back to gconf */
	setEntryKeyList(keyprefix, newlist);
	
	g_slist_free(list);
	g_slist_free(newlist);
}

/* compare function for numerical GSList sorting in getFreeEntryKey() */
static gint compare_func(gconstpointer a, gconstpointer b) {
	GError		*err = NULL;
	GConfValue	*element;
	const char	*feedkey1, *feedkey2;	
	gint		result, res;
	
	element = (GConfValue *)a;
	feedkey1 = gconf_value_get_string(element);
	is_gconf_error(err);
	
	element = (GConfValue *)b;
	feedkey2 = gconf_value_get_string(element);
	is_gconf_error(err);	
	
	result = atoi(feedkey1) - atoi(feedkey2);
	res = (result)?result/abs(result):0;

	return res;
}

/* this method is used to get a feedkey which is not already
   used for an existing feed */
gchar * getFreeEntryKey(gchar *keyprefix) {
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
				
	iter = getEntryKeyList(keyprefix);
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

	/* last step: save directory title */	
	gconfpath = g_strdup_printf("%s/%s/feedlistname", PATH, newdirkey);
	setStringConfValue(gconfpath, title);
	g_free(gconfpath);

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

	g_slist_free(list);
	g_slist_free(newlist);
}

/* adds the given URL to the list of feeds... */
gchar * addEntryToConfig(gchar *keyprefix, gchar *url, gint type) {
	GError		*err = NULL;
	GConfValue	*newkey = NULL;
	GSList		*list, *newlist;
	gchar		*key;
	int 		error = 0;

	/* get available key */
	if(NULL == (key = getFreeEntryKey(keyprefix))) {
		g_print(_("error! could not get a free entry key!\n"));
		return NULL;
	}

	/* save feed url */	
	if(0 == setEntryURLInConfig(key, url)) {

		if(0 != setEntryTypeInConfig(key, type)) {
			g_print(_("error! could not set a URL for this key!\n"));
		}
		
		/* add feedkey to feedlist */
		list = getEntryKeyList(keyprefix);
		
		newkey = gconf_value_new(GCONF_VALUE_STRING);
		gconf_value_set_string(newkey, key);
		is_gconf_error(err);
		
		newlist = g_slist_append(list, newkey);
		
		/* write new list back to gconf */
		setEntryKeyList(keyprefix, newlist);
		
		g_free(newkey);
		g_slist_free(newlist);
	} else {
		g_print(_("error! could not set a URL for this key!\n"));	
	}
	
	return key;
}

int setEntryTitleInConfig(gchar *key, gchar *title) {
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

int setEntryTypeInConfig(gchar *key, gint type) {
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

int setEntryURLInConfig(gchar *key, gchar *url) {
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
					
	/* update URL */
	gconfpath = g_strdup_printf("%s/%s/updateInterval", PATH, feedkey);
	gconf_client_set_int(client, gconfpath, interval, &err);
	g_free(gconfpath);	
	if(is_gconf_error(err))
		return 1;
	
	return 0;	
}

void moveUpEntryPositionInConfig(gchar *keyprefix, gchar *key) {
	GError		*err = NULL;
	GSList		*iter, *list;
	GConfValue	*element;
	const char	*actualkey;
	gboolean	found = FALSE;
	gint 		pos = 0;
	
	list = iter = getEntryKeyList(keyprefix);
	/* scan the list for the element to move and unlink it */
	while(iter != NULL) {
		element = iter->data;
		actualkey = gconf_value_get_string(element);
		is_gconf_error(err);		
	
		g_assert(NULL != actualkey);
		if(0 == strcmp(key, actualkey)) {
			list = g_slist_remove_link(list, iter);
			found = 1;
			break;
		}

		iter = g_slist_next(iter);
		pos ++;
	}
	
	if(found) {
		/* reinsert at next position */
		element = gconf_value_new(GCONF_VALUE_STRING);
		gconf_value_set_string(element, key);
		is_gconf_error(err);
		
		list = g_slist_insert(list, element, pos - 1);

		/* save it back to gconf */
		setEntryKeyList(keyprefix, list);
	} else {
		g_error("internal error, element should be moved but was not found in list!\n");
	}
	
	g_slist_free(list);
	
}

void moveDownEntryPositionInConfig(gchar *keyprefix, gchar *key) {
	GError		*err = NULL;
	GSList		*iter, *list;
	GConfValue	*element;
	const char	*actualkey;
	gboolean	found = FALSE;
	gint 		pos = 0;
	
	list = iter = getEntryKeyList(keyprefix);
	/* scan the list for the element to move and unlink it */
	while(iter != NULL) {
		element = iter->data;
		actualkey = gconf_value_get_string(element);
		is_gconf_error(err);		
	
		g_assert(NULL != actualkey);
		if(0 == strcmp(key, actualkey)) {
			list = g_slist_remove_link(list, iter);
			found = 1;
			break;
		}

		iter = g_slist_next(iter);
		pos ++;
	}
	
	if(found) {
		/* reinsert at next position */
		element = gconf_value_new(GCONF_VALUE_STRING);
		gconf_value_set_string(element, key);
		is_gconf_error(err);
		
		list = g_slist_insert(list, element, pos + 1);

		/* save it back to gconf */
		setEntryKeyList(keyprefix, list);
	} else {
		g_error("internal error, element should be moved but was not found in list!\n");
	}

	g_slist_free(list);	
}

void sortEntryKeyList(gchar *keyprefix) {
}

void loadEntries() {
	GError		*err = NULL;
	GSList		*groupiter = NULL, *iter = NULL;
	GConfValue	*element, *keylist, *groups;
	gchar		*gconfpath;
	gchar		*name, *url, *keyprefix, *keylisttitle;
	const char	*key;
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
		if(NULL == (keylisttitle = getStringConfValue(gconfpath)))
			keylisttitle = g_strdup(_("my feeds"));	 /* to avoid migration problems */
			
		g_free(gconfpath);
		
		addFolder(keyprefix, keylisttitle);
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
			type = gconf_client_get_int(client, gconfpath, &err);
			is_gconf_error(err);
			g_free(gconfpath);

			gconfpath = g_strdup_printf("%s/%s/url", PATH, key);
			url = gconf_client_get_string(client, gconfpath, &err);
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


			if(type == 0)
				type = FST_RSS;
				
			if(interval == 0)
				interval = -1;

			addEntry(type, url, (gchar *)key, keyprefix, name, interval);

			iter = g_slist_next(iter);
		}
			
		// FIXME: free keylist data with g_slist_free(...
		g_free(keylist);		
		groupiter = g_slist_next(groupiter);
	}
	
	g_slist_free(groupiter);
	g_free(groups);
		
	/* enforce background loading */
	updateNow();
}

/* returns true if namespace is enabled in configuration */
gboolean getNameSpaceStatus(gchar *nsname) {
	gchar		*gconfpath;
	gboolean	status;
	
	g_assert(NULL != nsname);
	
	gconfpath = g_strdup_printf("%s/ns_%s", PATH, nsname);
	status = getBooleanConfValue(gconfpath);
	g_free(gconfpath);
	
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

/* generic configuration access methods used for preferences */

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
	GError		*err = NULL;
	gboolean	value;

	g_assert(valuename != NULL);
		
	value = gconf_client_get_bool(client, valuename, &err);	
	is_gconf_error(err);
		
	return value;
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
	GError		*err = NULL;
	gchar		*value;

	g_assert(valuename != NULL);
		
	value = gconf_client_get_string(client, valuename, &err);	
	is_gconf_error(err);
	
	if(NULL == value)
		value = g_strdup("");
		
	return value;
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
	GError		*err = NULL;
	gint		value;

	g_assert(valuename != NULL);
		
	value = gconf_client_get_int(client, valuename, &err);	
	is_gconf_error(err);
		
	return value;
}
