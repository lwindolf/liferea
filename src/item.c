/*
   common item handling
   
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
   Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
		      
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

#include <glib.h>
#include <time.h>
#include <string.h> /* For memset() */

#include "cdf_item.h"
#include "rss_item.h"
#include "pie_entry.h"
#include "ocs_dir.h"
#include "vfolder.h"
#include "item.h"
#include "support.h"
#include "common.h"
#include "htmlview.h"
#include "callbacks.h"
#include "ui_tray.h"

/* function to create a new feed structure */
itemPtr item_new(void) {
	itemPtr		ip;
	
	ip = g_new0(struct item, 1);
	ip->readStatus = FALSE;
	ip->marked = FALSE;

	return ip;
}

gchar *	item_get_title(itemPtr ip) {return (ip != NULL ? ip->title : NULL); }
gchar *	item_get_description(itemPtr ip) { return (ip != NULL ? ip->description : NULL); }
gchar * item_get_source(itemPtr ip) { return (ip != NULL ? ip->source : NULL); }
time_t	item_get_time(itemPtr ip) { return (ip != NULL ? ip->time : 0); }
gboolean item_get_read_status(itemPtr ip) { return (ip != NULL ? ip->readStatus : FALSE); }
gboolean item_get_mark(itemPtr ip) { g_assert(ip != NULL); return ip->marked;}

void item_set_mark(itemPtr ip, gboolean flag) {
	ip->marked = flag;

	if (ip->ui_data != NULL)
	ui_update_item(ip);

	if (ip->fp != NULL)
		ip->fp->needsCacheSave = TRUE;
}

void item_set_unread(itemPtr ip) { 
	GSList		*vfolders;
	feedPtr		fp;
	
	if(TRUE == ip->readStatus) {
		feed_increase_unread_counter((feedPtr)(ip->fp));

		vfolders = ip->vfolders;
		while(NULL != vfolders) {
			fp = vfolders->data;
			feed_increase_unread_counter(fp);
			vfolders = g_slist_next(vfolders);
		}
		
		ip->readStatus = FALSE;
		if (ip->ui_data != NULL)
			ui_update_item(ip);
		if (ip->fp != NULL)
			ip->fp->needsCacheSave = TRUE;
	} 
}

void item_set_read(itemPtr ip) { 
	GSList		*vfolders;
	feedPtr		fp;

	if(FALSE == ip->readStatus) {
		feed_decrease_unread_counter((feedPtr)(ip->fp));

		vfolders = ip->vfolders;
		while(NULL != vfolders) {
			fp = vfolders->data;
			feed_decrease_unread_counter(fp);
			vfolders = g_slist_next(vfolders);
		}
		
		ip->readStatus = TRUE; 
		if (ip->ui_data)
			ui_update_item(ip);
		if (ip->fp != NULL)
			ip->fp->needsCacheSave = TRUE;
	}
	ui_tray_zero_new();
}

/* called when a item matches a vfolder... */
void addVFolderToItem(itemPtr ip, gpointer fp) {

	ip->vfolders = g_slist_append(ip->vfolders, fp);
}

/* called when a vfolder is removed, to remove it from the
   items vfolder list */
void removeVFolderFromItem(itemPtr ip, gpointer fp) {

	ip->vfolders = g_slist_remove(ip->vfolders, fp);
}

void item_free(itemPtr ip) {
	
	if(FALSE == ip->readStatus)
		feed_decrease_unread_counter(ip->fp);

	g_free(ip->title);
	g_free(ip->description);
	g_free(ip->source);
	g_free(ip->id);
	// FIXME: remove item from all assigned VFolders!
	if (ip->ui_data != NULL)
		ui_free_item_ui_data(ip);
	g_free(ip);
}

void item_display(itemPtr ip) {
	gchar	*buffer = NULL;
	
	ui_htmlview_start_output(&buffer, TRUE);
	addToHTMLBuffer(&buffer, ip->description);
	ui_htmlview_finish_output(&buffer);
	ui_htmlview_write(buffer);
	//g_free(buffer);
}

itemPtr item_parse_cache(xmlDocPtr doc, xmlNodePtr cur) {
	itemPtr 	ip;
	gchar		*tmp;
	
	g_assert(NULL != doc);
	g_assert(NULL != cur);
	
	ip = item_new();
	
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
 		tmp = utf8_fix(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
		
		if(tmp == NULL) {
			cur = cur->next;
			continue;
		}
		
		if(!xmlStrcmp(cur->name, BAD_CAST"title"))
			ip->title = g_strdup(tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"description"))
			ip->description = g_strdup(tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"source"))
			ip->source = g_strdup(tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"id"))
			ip->id = g_strdup(tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"readStatus"))
			ip->readStatus = (0 == atoi(tmp))?FALSE:TRUE;		

		else if(!xmlStrcmp(cur->name, BAD_CAST"mark")) 
			ip->marked = (1 == atoi(tmp))?TRUE:FALSE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"time"))
			ip->time = atol(tmp);
		
		g_free(tmp);	
		cur = cur->next;
	}
		
	return ip;
}
