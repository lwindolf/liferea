/**
 * @file metadata.c Metadata storage API
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <glib.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "metadata.h"
#include "xml.h"

/* Metadata in Liferea are ordered lists of key/value list pairs. Both 
   feed list nodes and items can have a list of metadata assigned. Metadata
   date values are always text values but maybe of different type depending
   on their usage type. */

/** Metadata value types */
enum {
	METADATA_TYPE_ASCII = 1,	/**< metadata can be any character data */
	METADATA_TYPE_URL = 2,		/**< metadata is an URL and guaranteed to be valid for use in XML */
	METADATA_TYPE_HTML = 3		/**< metadata is XHTML content and valid to be embedded in XML */
};

static GHashTable *metadata_types = NULL;

struct pair {
	gchar		*strid;		/** metadata type id */
	GSList		*data;		/** list of metadata values */
};

static void metadata_type_register(const gchar *name, gint type) {

	if(!metadata_types)
		metadata_types = g_hash_table_new(g_str_hash, g_str_equal);
		
	g_hash_table_insert(metadata_types, (gpointer)name, GINT_TO_POINTER(type));
}

static gint metadata_get_type(const gchar *name) {
	gint	type;

	type = GPOINTER_TO_INT(g_hash_table_lookup(metadata_types, (gpointer)name));
	if(0 == type)
		debug1(DEBUG_PARSING, "unknown metadata type (%s)", name);
	
	return type;
}

GSList * metadata_list_append(GSList *metadata, const gchar *strid, const gchar *data) {
	GSList		*iter = metadata;
	gchar		*tmp, *checked_data = NULL;
	struct pair 	*p;
	
	if(NULL == data)
		return metadata;
	
	/* lookup type and check format */
	switch(metadata_get_type(strid)) {
		case METADATA_TYPE_ASCII:
			/* No check because renderer will process further */
			checked_data = g_strdup(data);
			break;
		case METADATA_TYPE_URL:
			/* Simple sanity check to see if it doesn't break XML */
			if(!strchr(data, '<') && !(strchr(data, '>')) && !(strchr(data, '&'))) {
				checked_data = g_strdup(data);
			} else {
				checked_data = common_uri_escape(data);
			}
			break;
		default:
			debug1(DEBUG_CACHE, "Unknown metadata type \"%s\", this is a program bug! Treating as HTML.", strid);
		case METADATA_TYPE_HTML:
			/* Needs to check for proper XHTML */
			if (xhtml_is_well_formed (data)) {
				tmp = g_strdup (data);
			} else {
				debug1 (DEBUG_PARSING, "not well formed HTML: %s", data);
				tmp = g_markup_escape_text (data, -1);
				debug1 (DEBUG_PARSING, "escaped as: %s", tmp);
			}
			/* And needs to remove DHTML */
			checked_data = xhtml_strip_dhtml (tmp);
			g_free (tmp);
			break;
	}
	
	while(iter) {
		p = (struct pair*)iter->data; 
		if(g_str_equal(p->strid, strid)) {
			p->data = g_slist_append(p->data, checked_data);
			return metadata;
		}
		iter = iter->next;
	}
	p = g_new(struct pair, 1);
	p->strid = g_strdup(strid);
	p->data = g_slist_append(NULL, checked_data);
	metadata = g_slist_append(metadata, p);
	return metadata;
}

void metadata_list_set(GSList **metadata, const gchar *strid, const gchar *data) {
	GSList	*iter = *metadata;
	struct pair *p;
	
	while(iter) {
		p = (struct pair*)iter->data; 
		if(g_str_equal(p->strid, strid)) {
			if(p->data) {
				/* exchange old value */
				g_free(((GSList *)p->data)->data);
				((GSList *)p->data)->data = g_strdup(data);
			} else {
				p->data = g_slist_append(p->data, g_strdup(data));
			}
			return;
		}
		iter = iter->next;
	}
	p = g_new(struct pair, 1);
	p->strid = g_strdup(strid);
	p->data = g_slist_append(NULL, g_strdup(data));
	*metadata = g_slist_append(*metadata, p);
}

void metadata_list_foreach(GSList *metadata, metadataForeachFunc func, gpointer user_data) {
	GSList	*list = metadata;
	guint	index = 0;
	
	while(list) {
		struct pair *p = (struct pair*)list->data; 
		GSList *values = (GSList *)p->data;
		while(values) {
			index++;
			(*func)(p->strid, values->data, index, user_data);
			values = g_slist_next(values);
		}
		list = list->next;
	}
}

GSList * metadata_list_get_values(GSList *metadata, const gchar *strid) {
	GSList *list = metadata;
	
	while(list) {
		struct pair *p = (struct pair*)list->data; 
		if(g_str_equal(p->strid, strid))
			return p->data;
		list = list->next;
	}
	return NULL;
}

const gchar * metadata_list_get(GSList *metadata, const gchar *strid) {
	GSList	*values;
	
	values = metadata_list_get_values(metadata, strid);
	return values?values->data:NULL;

}

GSList * metadata_list_copy(GSList *list) {
	GSList		*copy = NULL;
	GSList		*list2, *iter2, *iter = list;
	struct pair	*p;
	
	while(iter) {
		p = (struct pair*)iter->data;
		iter2 = list2 = p->data;
		while(iter2) {
			copy = metadata_list_append(copy, p->strid, iter2->data);
			iter2 = iter2->next;
		}
		iter = iter->next;
	}
	
	return copy;
}

void metadata_list_free(GSList *metadata) {
	GSList		*list2, *iter2, *iter = metadata;
	struct pair	*p;
	
	while(iter != NULL) {
		p = (struct pair*)iter->data;
		list2 = p->data;
		iter2 = list2;
		while(iter2 != NULL) {
			g_free(iter2->data);
			iter2 = iter2->next;
		}
		g_slist_free(list2);
		g_free(p->strid);
		g_free(p);
		iter = iter->next;
	}
	g_slist_free(metadata);
}

void metadata_add_xml_nodes(GSList *metadata, xmlNodePtr parentNode) {
	GSList *list = metadata;
	xmlNodePtr attribute;
	xmlNodePtr metadataNode = xmlNewChild(parentNode, NULL, "attributes", NULL);
	
	while(list) {
		struct pair *p = (struct pair*)list->data; 
		GSList *list2 = p->data;
		while(list2) {
			attribute = xmlNewTextChild(metadataNode, NULL, "attribute", list2->data);
			xmlNewProp(attribute, "name", p->strid);
			list2 = list2->next;
		}
		list = list->next;
	}
}

GSList * metadata_parse_xml_nodes(xmlNodePtr cur) {
	xmlNodePtr	attribute = cur->xmlChildrenNode;
	GSList 		*metadata = NULL;
	
	while(attribute) {
		if(attribute->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp(attribute->name, BAD_CAST"attribute")) {
			xmlChar *name = xmlGetProp(attribute, BAD_CAST"name");
			if(name) {
				gchar *value = xmlNodeListGetString(cur->doc, attribute->xmlChildrenNode, TRUE);
				if(value) {
					metadata = metadata_list_append(metadata, name, value);
					xmlFree(value);
				}
				xmlFree(name);
			}
		}
		attribute = attribute->next;
	}
	return metadata;
}

void metadata_init(void) {

	/* register metadata types to check validity on adding */
	
	/* generic types */
	metadata_type_register("author",		METADATA_TYPE_HTML);
	metadata_type_register("contributor",		METADATA_TYPE_HTML);
	metadata_type_register("copyright",		METADATA_TYPE_HTML);
	metadata_type_register("language",		METADATA_TYPE_HTML);
	metadata_type_register("pubDate",		METADATA_TYPE_ASCII);
	metadata_type_register("contentUpdateDate",	METADATA_TYPE_ASCII);
	metadata_type_register("managingEditor",	METADATA_TYPE_HTML);
	metadata_type_register("webmaster",		METADATA_TYPE_HTML);
	metadata_type_register("feedgenerator",		METADATA_TYPE_HTML);
	metadata_type_register("imageUrl",		METADATA_TYPE_URL);
	metadata_type_register("textInput",		METADATA_TYPE_HTML);
	metadata_type_register("errorReportsTo",	METADATA_TYPE_HTML);
	metadata_type_register("feedgeneratorUri",	METADATA_TYPE_URL);
	metadata_type_register("category",		METADATA_TYPE_HTML);
	metadata_type_register("enclosure",		METADATA_TYPE_URL);
	metadata_type_register("commentsUri",		METADATA_TYPE_URL);
	metadata_type_register("commentFeedUri",	METADATA_TYPE_URL);
	metadata_type_register("feedTitle",		METADATA_TYPE_HTML);
	
	/* types for aggregation NS */
	metadata_type_register("agSource",		METADATA_TYPE_URL);
	metadata_type_register("agTimestamp",		METADATA_TYPE_ASCII);

	/* types for blog channel */
	metadata_type_register("blogChannel",		METADATA_TYPE_HTML);

	/* types for creative commons */
	metadata_type_register("license",		METADATA_TYPE_HTML);
	
	/* types for Dublin Core (some dc tags are mapped to feed and item metadata types) */
	metadata_type_register("creator",		METADATA_TYPE_HTML);
	metadata_type_register("publisher",		METADATA_TYPE_HTML);
	metadata_type_register("type",			METADATA_TYPE_HTML);
	metadata_type_register("format",		METADATA_TYPE_HTML);
	metadata_type_register("identifier",		METADATA_TYPE_HTML);
	metadata_type_register("source",		METADATA_TYPE_URL);
	metadata_type_register("coverage",		METADATA_TYPE_HTML);
	
	/* types for photo blogs */
	metadata_type_register("photo", 		METADATA_TYPE_URL);

	/* types for slash */
	metadata_type_register("slash",			METADATA_TYPE_HTML);

	return;
}
