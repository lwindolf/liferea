/*
   folder handling

   Copyright (C) 2003,2004 Lars Lindner <lars.lindner@gmx.net>
   
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

#include <gconf/gconf.h>	// FIXME

#include "support.h"
#include "common.h"
#include "conf.h"
#include "feed.h"
#include "folder.h"
#include "callbacks.h"	

extern GMutex * feeds_lock;

/* hash table to lookup the tree iterator for each key prefix */
GHashTable	*folders = NULL;

/* used to lookup a feed/folders pointer specified by a key */
extern GHashTable	*feeds;

extern GtkWidget	*mainwindow;

extern GtkTreeStore * getFeedStore(void);

/* ---------------------------------------------------------------------------- */
/* folder handling stuff (thats not the VFolder handling!)			*/
/* ---------------------------------------------------------------------------- */

void initFolders(void) {

	/* the folder code should reuse the feed list lock... */			
	folders =  g_hash_table_new(g_str_hash, g_str_equal);	
}

gchar * getFolderTitle(gchar *keyprefix) {
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter;
	gchar			*tmp_title;

	feedstore = getFeedStore();
	g_assert(feedstore != NULL);
	
	/* topiter must not be NULL! because we cannot rename the root folder ! */
	g_assert(NULL != folders);
	if(NULL != (iter = g_hash_table_lookup(folders, (gpointer)keyprefix))) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), iter, FS_TITLE, &tmp_title, -1);	
		return tmp_title;
	} else {
		g_warning(_("getFolderTitle() internal error! could not determine folder iterator for keyprefix \"%s\"!\n"), keyprefix);
		return NULL;
	}
}

void setFolderTitle(gchar *keyprefix, gchar *title) {
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter;

	feedstore = getFeedStore();
	g_assert(feedstore != NULL);
	
	/* topiter must not be NULL! because we cannot rename the root folder ! */
	g_assert(NULL != folders);
	if(NULL != (iter = g_hash_table_lookup(folders, (gpointer)keyprefix))) {
		gtk_tree_store_set(feedstore, iter, FS_TITLE, title, -1);	
		setFolderTitleInConfig(keyprefix, title);
	} else {
		g_warning(_("setFolderTitle() internal error! could not determine folder iter for keyprefix \"%s\"!\n"), keyprefix);
	}
}

void setFolderCollapseState(gchar *keyprefix, gboolean collapsed) {
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter;
	GtkTreePath		*path;
	GtkWidget		*treeview;	

	/* expand folder if necessary (they are collapsed by default */
	if(!collapsed) {
		feedstore = getFeedStore();
		g_assert(NULL != feedstore);
		
		g_assert(NULL != folders);
		if(NULL != (iter = g_hash_table_lookup(folders, (gpointer)keyprefix))) {
			if(NULL != (treeview = lookup_widget(mainwindow, "feedlist"))) {
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), iter);
				gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, TRUE);
				gtk_tree_path_free(path);
			}
		}
	}
}

static void saveFolderCollapseState(gpointer key, gpointer value, gpointer user_data) {
	gchar			*keyprefix = (gchar *)key;
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter = (GtkTreeIter *)value;
	GtkTreePath		*path;
	GtkWidget		*treeview;
		
	feedstore = getFeedStore();
	g_assert(NULL != feedstore);

	if(NULL != iter) {	/* true for root */
		if(NULL != (treeview = lookup_widget(mainwindow, "feedlist"))) {
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), iter);
			setFolderCollapseStateInConfig(keyprefix, !gtk_tree_view_row_expanded(GTK_TREE_VIEW(treeview), path));
			gtk_tree_path_free(path);
		}
	}
}

void saveAllFolderCollapseStates(void) {
	g_hash_table_foreach(folders, saveFolderCollapseState, NULL);
}

void addFolder(gchar *keyprefix, gchar *title, gint type) {
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter;

	/* check if a folder with this keyprefix already
	   exists to check config consistency */
	g_assert(NULL != folders);
	if(NULL != (iter = g_hash_table_lookup(folders, (gpointer)keyprefix))) {
		g_warning("There is already a folder with this keyprefix!\nYou may have an inconsistent configuration!\n");
		return;
	}

	if(NULL == (iter = (GtkTreeIter *)g_malloc(sizeof(GtkTreeIter)))) 
		g_error("could not allocate memory!\n");

	feedstore = getFeedStore();
	g_assert(feedstore != NULL);

	/* if keyprefix is "" we have the root folder and don't create
	   a new iter! */
	if(0 == strlen(keyprefix)) {
		iter = NULL;
	} else {
		gtk_tree_store_append(feedstore, iter, NULL);
		gtk_tree_store_set(feedstore, iter, FS_TITLE, title,
						    FS_KEY, keyprefix,	
						    FS_TYPE, type,
						    -1);
	}
					    
	g_hash_table_insert(folders, (gpointer)keyprefix, (gpointer)iter);
}

void removeFolder(gchar *keyprefix) {
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter;

	feedstore = getFeedStore();
	g_assert(feedstore != NULL);
	
	/* topiter must not be NULL! because we cannot delete the root folder ! */
	g_assert(NULL != folders);
	if(NULL != (iter = g_hash_table_lookup(folders, (gpointer)keyprefix))) {
		removeFolderFromConfig(keyprefix);
		g_hash_table_remove(folders, (gpointer)keyprefix);
	} else {
		g_warning(_("removeFolder() internal error! could not determine folder iter for keyprefix \"%s\"!\n"), keyprefix);
	}
}

/* this function is a workaround to the cant-drop-rows-into-emtpy-
   folders-problem, so we simply pack an (empty) entry into each
   empty folder like Nautilus does... */
   
static void checkForEmptyFolder(gpointer key, gpointer value, gpointer user_data) {
	GtkTreeStore	*feedstore;
	GtkTreeIter	iter;
	int		count;
	gint		tmp_type;
	gboolean	valid;
	
	/* this function does two things:
	
	   1. add "(empty)" entry to an empty folder
	   2. remove an "(empty)" entry from a non empty folder
	      (this state is possible after a drag&drop action) */

	feedstore = getFeedStore();
	g_assert(feedstore != NULL);
	      
	/* key is folder keyprefix, value is folder tree iterator */
	count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(feedstore), (GtkTreeIter *)value);
	
	/* case 1 */
	if(0 == count) {
		gtk_tree_store_append(feedstore, &iter, (GtkTreeIter *)value);
		gtk_tree_store_set(feedstore, &iter,
			   FS_TITLE, _("(empty)"),
			   FS_KEY, "empty",
			   FS_TYPE, FST_EMPTY,
			   -1);	
		return;
	}
	
	if(1 == count)
		return;
		
	/* else we could have case 2 */
	gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &iter, (GtkTreeIter *)value);
	do {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter, FS_TYPE, &tmp_type, -1);

		if(FST_EMPTY == tmp_type) {
			gtk_tree_store_remove(feedstore, &iter);
			return;
		}
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	} while(valid);
}

void checkForEmptyFolders(void) {
	g_hash_table_foreach(folders, checkForEmptyFolder, NULL);	
}

/* method to save the feed list of a folder specified by keyprefix */
void saveFolderFeedList(gchar *keyprefix) {
	GtkTreeIter	iter;
	GtkTreeIter	*topiter;
	GSList		*tmplist, *newkeylist = NULL;
	GConfValue	*new_value = NULL;
	GtkTreeStore	*feedstore;
	gint		tmp_type;
	gchar		*tmp_key;
	gboolean	valid;

	g_assert(NULL != folders);
	topiter = g_hash_table_lookup(folders, (gpointer)keyprefix);
	
	feedstore = getFeedStore();
	g_assert(feedstore != NULL);
	topiter = (GtkTreeIter *)g_hash_table_lookup(folders, keyprefix);
	valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &iter, topiter);
	while(valid) {			
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter,
				FS_KEY, &tmp_key,
				FS_TYPE, &tmp_type,
		  	     	-1);

		/* add key to new key list, if its no empty or help entry */
		if(!IS_NODE(tmp_type) && (tmp_type != FST_EMPTY) &&
		   (NULL == strstr(tmp_key, "help"))) {
			new_value = gconf_value_new(GCONF_VALUE_STRING);
			gconf_value_set_string(new_value, tmp_key);
			newkeylist = g_slist_append(newkeylist, new_value);
		}
		g_free(tmp_key);
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	}	

	setFeedKeyList(keyprefix, newkeylist);	
	
	tmplist = newkeylist;
	while(NULL != tmplist) {
		gconf_value_free(tmplist->data);
		tmplist = g_slist_next(tmplist);	
	}	
	g_slist_free(newkeylist);
}

/* function to scan folder with keyprefix for the feed with key, if the
   feed entry is found the entries configuration 'll be removed, then a
   new feed key for the actual folder 'll be generated and saved to 
   tree store and configuration (this function is called after a DND
   operation to update the DND modifieds feed key and keyprefix and
   its old and new folders keylist) */
static void moveIfInFolder(gpointer keyprefix, gpointer value, gpointer key) {
	GtkTreeIter	iter;
	GtkTreeIter	*topiter = (GtkTreeIter *)value;
	GtkTreeStore	*feedstore;
	gint		tmp_type;
	feedPtr		fp;
	gchar		*newkey, *tmp_key;
	gchar		*newfilename, *oldfilename;
	gboolean	valid, wasFound, found;

	g_assert(NULL != keyprefix);
	g_assert(NULL != key);
	
	feedstore = getFeedStore();
	g_assert(feedstore != NULL);

	g_assert(NULL != folders);
	topiter = (GtkTreeIter *)g_hash_table_lookup(folders, keyprefix);
	valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &iter, topiter);
	found = FALSE;
	wasFound = FALSE;
	while(valid) {
		found = FALSE;
				
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter,
				FS_KEY, &tmp_key,
				FS_TYPE, &tmp_type,
		  	     	-1);

		if(!IS_NODE(tmp_type)) {
			g_assert(NULL != tmp_key);
			if(0 == strcmp(tmp_key, (gchar *)key)) {
				found = TRUE;
			}
		}

		if(found) {
			wasFound = TRUE;
			g_assert(NULL != feeds);
			fp = (feedPtr)g_hash_table_lookup(feeds, (gpointer)key);
			g_assert(NULL != fp);
			newkey = addFeedToConfig((gchar *)keyprefix,
						  (gchar *)getFeedSource(fp),
						  tmp_type);

			/* rename cache file/directory */
			oldfilename = getCacheFileName(fp->keyprefix, tmp_key, getExtension(tmp_type));
			newfilename = getCacheFileName(keyprefix, newkey, getExtension(tmp_type));

			g_assert(NULL != oldfilename);
			g_assert(NULL != newfilename);
				
			if(0 != rename(oldfilename, newfilename)) {
				g_print(_("error! could not move cache file %s to file %s\n"), oldfilename, newfilename);
			}
			g_free(oldfilename);
			g_free(newfilename);		

			/* rename cache favicon */
			oldfilename = getCacheFileName(fp->keyprefix, tmp_key, "xpm");
			newfilename = getCacheFileName(keyprefix, newkey, "xpm");
			
			rename(oldfilename, newfilename);
			
			g_free(oldfilename);
			g_free(newfilename);
		
			g_mutex_lock(feeds_lock);	/* prevent any access during the feed structure modifications */

			/* move key in configuration */
			removeFeedFromConfig(fp->keyprefix, key);	/* delete old one */
			fp->key = newkey;				/* update feed structure key */
			fp->keyprefix = keyprefix;
			g_hash_table_insert(feeds, (gpointer)newkey, (gpointer)fp);	/* update in feed list */
			g_hash_table_remove(feeds, (gpointer)key);

			g_mutex_unlock(feeds_lock);
			
			/* write feed properties to new key */
			setFeedTitleInConfig(newkey, getFeedTitle(fp));
			if(IS_FEED(tmp_type))
				setFeedUpdateIntervalInConfig(newkey, getFeedUpdateInterval(fp));

			/* update changed row contents */
			gtk_tree_store_set(feedstore, &iter, FS_KEY, (gpointer)newkey, -1);
		}
		g_free(tmp_key);
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	}

	/* if we found the new entry, we have to save the new folder
	   contents order */
	if(wasFound)
		saveFolderFeedList(keyprefix);
}

/* function to reflect DND of feed entries in the configuration */
void moveInFeedList(gchar *oldkeyprefix, gchar *oldkey) {

	/* find new treestore entry and keyprefix */
	g_hash_table_foreach(folders, moveIfInFolder, (gpointer)oldkey);
}

