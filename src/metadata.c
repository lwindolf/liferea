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
#include "support.h"
#include "htmlview.h"
#include "metadata.h"
static GHashTable *strtoattrib;

struct attribute {
	gchar *strid;
	
	renderHTMLFunc renderhtmlfunc;
	
	gpointer user_data;
};

struct pair {
	struct attribute *attrib;
	GSList *data;
};

static void attribs_init();

void metadata_init() {
	strtoattrib = g_hash_table_new(g_str_hash, g_str_equal);
	attribs_init();
}

void metadata_register(const gchar *strid, renderHTMLFunc renderfunc, gpointer user_data) {
	struct attribute *attrib = g_new(struct attribute, 1);
	
	attrib->strid = g_strdup(strid);
	attrib->renderhtmlfunc = renderfunc;
	attrib->user_data = user_data;
	
	g_hash_table_insert(strtoattrib, attrib->strid, attrib);
}

static void metadata_render(struct attribute *attrib, struct displayset *displayset, gpointer data) {
	
	attrib->renderhtmlfunc(data, displayset, attrib->user_data);
}

gpointer metadata_list_append(gpointer metadata_list, const gchar *strid, const gchar *data) {
	struct attribute *attrib = g_hash_table_lookup(strtoattrib, strid);
	GSList *list = (GSList*)metadata_list;
	GSList *iter = list;
	struct pair *p;
	
	while (iter != NULL) {
		p = (struct pair*)iter->data; 
		if (p->attrib == attrib) {
			p->data = g_slist_append(p->data, g_strdup(data));
			return list;
		}
		iter = iter->next;
	}
	p = g_new(struct pair, 1);
	p->attrib = attrib;
	p->data = g_slist_append(NULL, g_strdup(data));
	list = g_slist_append(list, p);
	return list;
}

void metadata_list_render(gpointer metadataList, struct displayset *displayset) {
	GSList *list = (GSList*)metadataList;
	
	while (list != NULL) {
		struct pair *p = (struct pair*)list->data; 
		GSList *list2 = p->data;
		while (list2 != NULL) {
			metadata_render(p->attrib, displayset, (gchar*)list2->data);
			list2 = list2->next;
		}
		list = list->next;
	}
}

/* Now comes the stuff to define particular attributes */

typedef enum {
	POS_HEAD,
	POS_BODY,
	POS_FOOT
} output_position;

struct str_attrib {
	output_position pos;
	gchar *prompt;
};

static void str_render(gpointer data, struct displayset *displayset, gpointer user_data) {
	struct str_attrib *props = (struct str_attrib*)user_data;
	gchar *str;
	switch (props->pos) {
	case POS_HEAD:
		str = g_strdup_printf(HEAD_LINE, props->prompt, (gchar*)data);;
		addToHTMLBufferFast(&(displayset->headtable), str);
		g_free(str);
		break;
	case POS_BODY:
		addToHTMLBufferFast(&(displayset->body), str);
		break;
	case POS_FOOT:
		FEED_FOOT_WRITE(displayset->foottable, props->prompt, (gchar*)data);
		break;
	}

}

#define REGISTER_STR_ATTRIB(position, strid, promptStr) do { \
 struct str_attrib *props = g_new(struct str_attrib, 1); \
 props->pos = (position); \
 props->prompt = _(promptStr); \
 metadata_register(strid, str_render, props); \
} while (0);

static void attribs_init() {
	REGISTER_STR_ATTRIB(POS_FOOT, "author", "author");
	REGISTER_STR_ATTRIB(POS_FOOT, "contributor", "contributors");
}
