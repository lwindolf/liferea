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
#include <libxml/tree.h>
#include "support.h"
#include "htmlview.h"
#include "metadata.h"
#include "common.h"
#include "ns_slash.h"

/* HTML definitions used for standard metadata rendering */
#define	IMG_START	"<img class=\"feed\" src=\""
#define IMG_END		"\"><br>"

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
static void attribs_register_default_renderer(const gchar *strid);

void metadata_init() {
	strtoattrib = g_hash_table_new(g_str_hash, g_str_equal);
	attribs_init();
}

void metadata_register_renderer(const gchar *strid, renderHTMLFunc renderfunc, gpointer user_data) {
	struct attribute *attrib = g_new(struct attribute, 1);
	
	if (g_hash_table_lookup(strtoattrib, strid) != NULL) {
		g_warning("Duplicate attribute was attempted to be registered: %s", strid);
		return;
	}

	attrib->strid = g_strdup(strid);
	attrib->renderhtmlfunc = renderfunc;
	attrib->user_data = user_data;
	
	g_hash_table_insert(strtoattrib, attrib->strid, attrib);
}

static void metadata_render(struct attribute *attrib, struct displayset *displayset, gpointer data) {
	
	attrib->renderhtmlfunc(data, displayset, attrib->user_data);
}

GSList * metadata_list_append(GSList *metadata, const gchar *strid, const gchar *data) {
	struct attribute *attrib;
	GSList	*iter = metadata;
	struct pair *p;
	
	if(NULL == (attrib = g_hash_table_lookup(strtoattrib, strid))) {
		g_warning("Encountered unknown attribute type \"%s\". This is a program bug.", strid);
		attribs_register_default_renderer(strid);
		attrib = g_hash_table_lookup(strtoattrib, strid);
	}
	
	while(iter != NULL) {
		p = (struct pair*)iter->data; 
		if(p->attrib == attrib) {
			p->data = g_slist_append(p->data, g_strdup(data));
			return metadata;
		}
		iter = iter->next;
	}
	p = g_new(struct pair, 1);
	p->attrib = attrib;
	p->data = g_slist_append(NULL, g_strdup(data));
	metadata = g_slist_append(metadata, p);
	return metadata;
}

void metadata_list_set(GSList **metadata, const gchar *strid, const gchar *data) {
	struct attribute *attrib;
	GSList	*iter = *metadata;
	struct pair *p;
	
	if(NULL == (attrib = g_hash_table_lookup(strtoattrib, strid))) {
		g_warning("Encountered unknown attribute type \"%s\". This is a program bug.", strid);
		attribs_register_default_renderer(strid);
		attrib = g_hash_table_lookup(strtoattrib, strid);
	}
	
	while(iter != NULL) {
		p = (struct pair*)iter->data; 
		if(p->attrib == attrib) {
			if(NULL != p->data) {
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
	p->attrib = attrib;
	p->data = g_slist_append(NULL, g_strdup(data));
	*metadata = g_slist_append(*metadata, p);
}

void metadata_list_render(GSList* metadata, struct displayset *displayset) {
	GSList *list = metadata;
	
	while(list != NULL) {
		struct pair *p = (struct pair*)list->data; 
		GSList *list2 = p->data;
		while(list2 != NULL) {
			metadata_render(p->attrib, displayset, (gchar*)list2->data);
			list2 = list2->next;
		}
		list = list->next;
	}
}

void metadata_list_free(GSList *metadata) {
	GSList *iter = metadata;
	
	while(iter != NULL) {
		struct pair *p = (struct pair*)iter->data;
		GSList *list2 = p->data;
		GSList *iter2 = list2;
		while(iter2 != NULL) {
			g_free(iter2->data);
			iter2 = iter2->next;
		}
		g_slist_free(list2);
		g_free(p);
		iter = iter->next;
	}
	g_slist_free(metadata);
}

void metadata_add_xml_nodes(GSList *metadata, xmlNodePtr parentNode) {
	GSList *list = metadata;
	xmlNodePtr attribute;
	xmlNodePtr metadataNode = xmlNewChild(parentNode, NULL, "attributes", NULL);
	
	while(list != NULL) {
		struct pair *p = (struct pair*)list->data; 
		GSList *list2 = p->data;
		while (list2 != NULL) {
			attribute = xmlNewTextChild(metadataNode, NULL, "attribute", list2->data);
			xmlNewProp(attribute, "name", p->attrib->strid);
			list2 = list2->next;
		}
		list = list->next;
	}
}

GSList * metadata_parse_xml_nodes(xmlDocPtr doc, xmlNodePtr cur) {
	xmlNodePtr	attribute = cur->xmlChildrenNode;
	GSList 		*metadata = NULL;
	
	while(attribute != NULL) {
		if (attribute->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp(attribute->name, BAD_CAST"attribute")) {
			xmlChar *name = xmlGetProp(attribute, "name");
			if (name != NULL) {
				gchar *value = xmlNodeListGetString(doc, attribute->xmlChildrenNode, TRUE);
				if(value != NULL) {
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

/* Now comes the stuff to define particular attributes */

typedef enum {
	POS_HEADTABLE,
	POS_HEAD,
	POS_BODY,
	POS_FOOTTABLE
} output_position;

struct str_attrib {
	output_position pos;
	gchar *prompt;
};

static void attribs_render_str(gpointer data, struct displayset *displayset, gpointer user_data) {
	struct str_attrib *props = (struct str_attrib*)user_data;
	gchar *str;
	switch (props->pos) {
	case POS_HEADTABLE:
		str = g_strdup_printf(HEAD_LINE, props->prompt, (gchar*)data);;
		addToHTMLBufferFast(&(displayset->headtable), str);
		g_free(str);
		break;
	case POS_HEAD:
		addToHTMLBufferFast(&(displayset->head), str);
		break;
	case POS_BODY:
		addToHTMLBufferFast(&(displayset->body), str);
		break;
	case POS_FOOTTABLE:
		FEED_FOOT_WRITE(displayset->foottable, props->prompt, (gchar*)data);
		break;
	}
}

static void attribs_render_image(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar *tmp = g_strdup_printf("<p>" IMG_START "%s" IMG_END "</p>", (gchar*)data);
	addToHTMLBufferFast(&(displayset->head), tmp);
	g_free(tmp);
}

static void attribs_render_foot_text(gpointer data, struct displayset *displayset, gpointer user_data) {
	addToHTMLBufferFast(&(displayset->foot), (gchar*)data);
}

static void attribs_render_comments_uri(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar *tmp;
	
	tmp = g_strdup_printf("<div style=\"margin-top:5px;margin-bottom:5px;\">(<a href=\"%s\">%s</a>)</div>", 
	                      (gchar*)data, _("comments"));
	
	addToHTMLBufferFast(&(displayset->foot), tmp);
	g_free(tmp);
}

static void attribs_render_enclosure(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar *tmp;
	gchar *tmp2; 
	
	tmp = g_strdup_printf(_("enclosed file: <a href=\"%s\">%s</a>"), (gchar *)data, (gchar *)data);
	tmp2 = g_strdup_printf("<div style=\"margin-top:5px;margin-bottom:5px;padding-left:5px;padding-right"
	                       ":5px;border-color:black;border-style:solid;border-width:1px;"
	                       "background-color:#E0E0E0\">%s</div>", tmp);
	addToHTMLBufferFast(&(displayset->foot), tmp2);
	g_free(tmp);
	g_free(tmp2);
}

static void attribs_render_feedgenerator_uri(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar *tmp = g_strdup_printf("<a href=\"%s\">%s</a>", (gchar*)data, (gchar*)data);
	FEED_FOOT_WRITE(displayset->foottable, _("feed generator"), tmp);
	g_free(tmp);
}

#define REGISTER_SIMPLE_ATTRIBUTE(position, strid, promptStr) do { \
 struct str_attrib *props = g_new(struct str_attrib, 1); \
 props->pos = (position); \
 props->prompt = (promptStr); \
 metadata_register_renderer(strid, attribs_render_str, props); \
} while (0);

static void attribs_init() {

	/* attributes resulting from general feed parsing */
	REGISTER_SIMPLE_ATTRIBUTE(POS_HEADTABLE, "feedTitle", _("Feed:"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_HEADTABLE, "itemTitle", _("Item:"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_HEADTABLE, "feedSource", _("Source:"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_HEADTABLE, "itemSource", _("Source:"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "author", _("author"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "contributor", _("contributors"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "copyright", _("copyright"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "language", _("language"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "feedUpdateDate", _("feed last updated"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "contentUpdateDate", _("content last updated"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "managingEditor", _("managing editor"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "webmaster", _("webmaster"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "category", _("category"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "feedgenerator", _("feed generator"));
	
	metadata_register_renderer("textInput", attribs_render_foot_text, NULL);
	metadata_register_renderer("commentsUri", attribs_render_comments_uri, NULL);
	metadata_register_renderer("enclosure", attribs_render_enclosure, NULL);
	
	/* types for admin */
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "errorReportsTo", _("report errors to"));
	metadata_register_renderer("feedgeneratorUri", attribs_render_feedgenerator_uri, NULL);
	
	/* types for aggregation */
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "agSource", _("original source"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "agTimestamp", _("original time"));

	/* types for blog channel */
	metadata_register_renderer("blogChannel", attribs_render_foot_text, NULL);
	
	/* types for creative commons */
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "license", _("license"));
	
	/* types for Dublin Core (some dc tags are mapped to feed and item metadata types) */
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "creator", _("creator"));	
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "publisher", _("publisher"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "type", _("type"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "format", _("format"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "identifier", _("identifier"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "source", _("source"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "coverage", _("coverage"));
	
	/* types for freshmeat */
	metadata_register_renderer("fmScreenshot", attribs_render_image, NULL);	

	/* types for slash */
	metadata_register_renderer("slash", ns_slash_render, NULL);	
}

static void attribs_register_default_renderer(const gchar *strid) {
	gchar *str = g_strdup(strid);
	
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, str, str);
}
