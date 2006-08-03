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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <libxml/tree.h>
#include <string.h>
#include "support.h"
#include "metadata.h"
#include "common.h"
#include "debug.h"

struct pair {
	gchar		*strid;		/** metadata type id */
	GSList		*data;		/** list of metadata values */
};

GSList * metadata_list_append(GSList *metadata, const gchar *strid, const gchar *data) {
	struct attribute *attrib;
	GSList	*iter = metadata;
	struct pair *p;
	
	while(iter) {
		p = (struct pair*)iter->data; 
		if(g_str_equal(p->strid, strid)) {
			p->data = g_slist_append(p->data, g_strdup(data));
			return metadata;
		}
		iter = iter->next;
	}
	p = g_new(struct pair, 1);
	p->strid = g_strdup(strid);
	p->data = g_slist_append(NULL, g_strdup(data));
	metadata = g_slist_append(metadata, p);
	return metadata;
}

void metadata_list_set(GSList **metadata, const gchar *strid, const gchar *data) {
	struct attribute *attrib;
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

GSList * metadata_list_get(GSList *metadata, const gchar *strid) {
	GSList *list = metadata;
	
	while(list) {
		struct pair *p = (struct pair*)list->data; 
		if(g_str_equal(p->strid, strid))
			return p->data;
		list = list->next;
	}
	return NULL;
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

static void attribs_init() {

	return;
	
	// FIXME: remove me

	/* attributes resulting from general feed parsing 
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "author", _("author"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "contributor", _("contributors"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "copyright", _("copyright"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "language", _("language"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "pubDate", _("feed published on"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "contentUpdateDate", _("content last updated"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "managingEditor", _("managing editor"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "webmaster", _("webmaster"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "feedgenerator", _("feed generator"));
	metadata_register_renderer("imageUrl",	attribs_render_image, FALSE, GINT_TO_POINTER(POS_HEAD));
	metadata_register_renderer("textInput",	attribs_render_foot_text, FALSE, NULL);
	
	/* types for admin 
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "errorReportsTo", _("report errors to"));
	metadata_register_renderer("feedgeneratorUri", attribs_render_feedgenerator_uri, FALSE, NULL);
	
	/* types for aggregation 
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "agSource", _("original source"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "agTimestamp", _("original time"));

	/* types for blog channel 
	metadata_register_renderer("blogChannel", attribs_render_foot_text, FALSE, NULL);
	
	/* types for creative commons 
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "license", _("license"));
	
	/* types for Dublin Core (some dc tags are mapped to feed and item metadata types) 
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "creator", _("creator"));	
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "publisher", _("publisher"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "type", _("type"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "format", _("format"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "identifier", _("identifier"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "source", _("source"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "coverage", _("coverage"));
	
	/* types for freshmeat 
	metadata_register_renderer("fmScreenshot", attribs_render_image, FALSE, GINT_TO_POINTER(POS_BODY));
	
	/* types for photo blogs 
	metadata_register_renderer("photo", ns_photo_render, FALSE, NULL);

	/* types for slash 
	metadata_register_renderer("slash", ns_slash_render, FALSE, NULL);	 /* This one should only be set, not appended */
}
