/**
 * @file metadata.c Metadata storage API
 *
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <glib.h>
#include "htmlview.h"
#include "metadata.h"
//static int next_id=1; /**< Stores the next numeric ID to be assigned*/

static GHashTable *strtoattrib;
//static GHashTable *numtoattrib;

struct attribute {
	//	gint numid;
	gchar *strid;
	
	parserFunc parserfunc;
	renderHTMLFunc renderhtmlfunc;
	freeDataFunc freefunc;
	
	gpointer user_data;
};

struct pair {
	struct attribute *attrib;
	gpointer data;
};

void metadata_init() {
	strtoattrib = g_hash_table_new(g_str_hash, g_str_equal);
	//numtoattrib = g_hash_table_new(g_int_hash, g_int_equal);
}

void metadata_register(const gchar *strid, parserFunc pfunc, renderHTMLFunc renderfunc, freeDataFunc freefunc, gpointer user_data) {
	struct attribute *attrib = g_new(struct attribute, 1);
	
	//attrib->numid = next_id++;
	attrib->strid = g_strdup(strid);
	attrib->parserfunc = pfunc;
	attrib->renderhtmlfunc = renderfunc;
	attrib->freefunc = freefunc;
	attrib->user_data = user_data;
	
	g_hash_table_insert(strtoattrib, attrib->strid, attrib);
	//g_hash_table_insert(numtoattrib, &(attrib->numid), attrib);
}

static gpointer metadata_parse(struct attribute *attrib, gpointer prevData, const gchar *str) {
	//struct attribute attrib = g_hash_table_lookup(numtoattrib, &numid);
	
	return attrib->parserfunc(prevData, str, attrib->user_data);
}

static void metadata_render(struct attribute *attrib, struct displayset *displayset, gpointer data) {
	
	attrib->renderhtmlfunc(data, displayset, attrib->user_data);
}

static gpointer metadata_list_insert(gpointer metadata_list, struct attribute *attrib, const gchar *data) {
	GSList *list = (GSList*)metadata_list;
	GSList *iter = list;
	struct pair *p;
	
	while (iter != NULL) {
		p = (struct pair*)iter->data; 
		if (p->attrib == attrib) {
			p->data = metadata_parse(attrib, p->data, data);
			return list;
		}
		iter = iter->next;
	}
	p = g_new(struct pair, 1);
	p->attrib = attrib;
	p->data = metadata_parse(attrib, NULL, data);
	list = g_slist_prepend(list, p);
	return list;
}

gpointer metadata_list_insert_strid(gpointer metadataList, const gchar *strid, const gchar *data) {
	struct attribute *attrib = g_hash_table_lookup(strtoattrib, strid);
	return metadata_list_insert(metadataList, attrib, data);
}

void metadata_list_render(gpointer metadataList, struct displayset *displayset) {
	GSList *list = (GSList*)metadataList;
	printf("Rendering stuff\n");
	while (list != NULL) {
		struct pair *p = (struct pair*)list->data; 
		
		metadata_render(p->attrib, displayset, p->data);
		list = list->next;
	}
}
