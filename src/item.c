/*
   common item handling
   
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

#include <glib.h>
#include <time.h>

#include "cdf_item.h"
#include "rss_item.h"
#include "pie_entry.h"
#include "ocs_dir.h"
#include "vfolder.h"
#include "item.h"
#include "support.h"
#include "common.h"

/* function to create a new feed structure */
itemPtr getNewItemStruct(void) {
	itemPtr		ip;
	
	/* initialize channel structure */
	if(NULL == (ip = (itemPtr) malloc(sizeof(struct item)))) {
		g_error("not enough memory!\n");
		exit(1);
	}
	
	memset(ip, 0, sizeof(struct item));
	ip->readStatus = FALSE;
	ip->marked = FALSE;
	ip->type = FST_INVALID;
	
	return ip;
}

gchar *	getItemTitle(itemPtr ip) { return ip->title; }
gchar *	getItemDescription(itemPtr ip) { return ip->description; }
gchar * getItemSource(itemPtr ip) { return ip->source; }
time_t	getItemTime(itemPtr ip) { return ip->time; }
gboolean getItemReadStatus(itemPtr ip) { return ip->readStatus; }
gboolean getItemMark(itemPtr ip) { return ip->marked; }
void setItemMark(itemPtr ip, gboolean flag) { ip->marked = flag; }

void markItemAsUnread(itemPtr ip) { 
	GSList		*vfolders;
	feedPtr		fp;
	
	if(TRUE == ip->readStatus) {
		increaseUnreadCount((feedPtr)(ip->fp));

		vfolders = ip->vfolders;
		while(NULL != vfolders) {
			fp = vfolders->data;
			increaseUnreadCount(fp);
			vfolders = g_slist_next(vfolders);
		}
		
		ip->readStatus = FALSE;
	} 
}

void markItemAsRead(itemPtr ip) { 
	GSList		*vfolders;
	feedPtr		fp;

	if(FALSE == ip->readStatus) {
		decreaseUnreadCount((feedPtr)(ip->fp));

		vfolders = ip->vfolders;
		while(NULL != vfolders) {
			fp = vfolders->data;
			decreaseUnreadCount(fp);
			vfolders = g_slist_next(vfolders);
		}
		
		ip->readStatus = TRUE; 
	}
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

void freeItem(itemPtr ip) {

	if(FALSE == ip->readStatus)
		decreaseUnreadCount(ip->fp);

	g_free(ip->title);
	g_free(ip->description);
	g_free(ip->source);
	g_free(ip->id);
	// FIXME: remove item from all assigned VFolders!
	g_free(ip);
}

void displayItem(itemPtr ip) {

	startHTMLOutput();
	writeHTML(ip->description);
	finishHTMLOutput();
	
	markItemAsRead(ip);
}

itemPtr parseCacheItem(xmlDocPtr doc, xmlNodePtr cur) {
	itemPtr ip;
	gchar	*tmp;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}
	
	ip = getNewItemStruct();
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"title"))
			ip->title = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
		if (!xmlStrcmp(cur->name, (const xmlChar *)"description"))
			ip->description = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
		if (!xmlStrcmp(cur->name, (const xmlChar *)"source"))
			ip->source = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
		if (!xmlStrcmp(cur->name, (const xmlChar *)"id"))
			ip->id = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
			
		if (!xmlStrcmp(cur->name, (const xmlChar *)"readStatus")) {
			tmp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			ip->readStatus = (0 == atoi(tmp))?FALSE:TRUE;
			xmlFree(tmp);
		}

		if (!xmlStrcmp(cur->name, (const xmlChar *)"mark")) {
			tmp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			ip->marked = (1 == atoi(tmp))?TRUE:FALSE;
			xmlFree(tmp);
		}
				
		if (!xmlStrcmp(cur->name, (const xmlChar *)"time")) {
			tmp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);			
			ip->time = atol(tmp);
			xmlFree(tmp);
		}
			
		cur = cur->next;
	}
		
	return ip;
}
