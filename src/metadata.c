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
#include "parsers/ns_slash.h"
#include "parsers/ns_photo.h"
#include "ui/ui_htmlview.h"

/* HTML definitions used for standard metadata rendering */
#define	IMG_START	"<img class=\"feed\" src=\""
#define IMG_END		"\" /><br />"

/** rendering method lookup hash */
static GHashTable *strtoattrib;

/** metadata rendering method registration structure */
struct attribute {
	gchar		*strid;		/** metadata type id */
	
	renderHTMLFunc	renderhtmlfunc;	/** function pointer to rendering function */
	gpointer	user_data;	/** user data passed to rendering function */
	
	gboolean	collapse;	/** if TRUE the rendering function will be called
					    once for all values, if FALSE it will be called
					    once for each value */
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

void metadata_register_renderer(const gchar *strid, renderHTMLFunc renderfunc, gboolean collapse, gpointer user_data) {
	struct attribute *attrib = g_new(struct attribute, 1);
	
	if(g_hash_table_lookup(strtoattrib, strid)) {
		g_warning("Duplicate attribute was attempted to be registered: %s", strid);
		return;
	}

	attrib->strid		= g_strdup(strid);
	attrib->renderhtmlfunc	= renderfunc;
	attrib->collapse	= collapse;
	attrib->user_data	= user_data;
	
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
		g_warning("Encountered unknown attribute type \"%s\".", strid);
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

GSList * metadata_list_get(GSList *metadata, const gchar *strid) {
	GSList *list = metadata;
	
	while(list != NULL) {
		struct pair *p = (struct pair*)list->data; 
		if(0 == strcmp(p->attrib->strid, strid))
			return p->data;
		list = list->next;
	}
	return NULL;
}

void metadata_list_render(GSList *metadata, struct displayset *displayset) {
	GSList *list = metadata;
	
	while(list != NULL) {
		struct pair *p = (struct pair*)list->data; 
		if(p->attrib->collapse) {
			/* render all values at once */
			metadata_render(p->attrib, displayset, p->data);
		} else {
			/* render each value independently */
			GSList *list2 = p->data;
			while(list2 != NULL) {
				metadata_render(p->attrib, displayset, (gchar*)list2->data);
				list2 = list2->next;
			}
		}
		list = list->next;
	}
}

GSList * metadata_list_copy(GSList *list) {
	GSList		*copy = NULL;
	GSList		*list2, *iter2, *iter = list;
	struct pair	*p;
	
	while(iter != NULL) {
		p = (struct pair*)iter->data;
		iter2 = list2 = p->data;
		while(iter2 != NULL) {
			copy = metadata_list_append(copy, (p->attrib)->strid, iter2->data);
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
	struct str_attrib	*props = (struct str_attrib*)user_data;
	gchar			*tmp, *escapedStr;
	
	escapedStr = g_markup_escape_text((gchar*)data, -1);
	switch(props->pos) {
		case POS_HEADTABLE:
			tmp = g_strdup_printf(HEAD_LINE, props->prompt, escapedStr);;
			addToHTMLBufferFast(&(displayset->headtable), tmp);
			g_free(tmp);
			break;
		case POS_HEAD:
			addToHTMLBufferFast(&(displayset->head), escapedStr);
			break;
		case POS_BODY:
			addToHTMLBufferFast(&(displayset->body), escapedStr);
			break;
		case POS_FOOTTABLE:
			FEED_FOOT_WRITE(displayset->foottable, props->prompt, escapedStr);
			break;
	}
	g_free(escapedStr);
}

static void attribs_render_image(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar *tmp;
	
	tmp = g_markup_printf_escaped("<p>" IMG_START "%s" IMG_END "</p>", (gchar*)data);	
	switch(GPOINTER_TO_INT(user_data)) {
		case POS_HEAD:
			addToHTMLBufferFast(&(displayset->head), tmp);
			break;
		case POS_BODY:
			addToHTMLBufferFast(&(displayset->body), tmp);
			break;
		default:
			debug0(DEBUG_GUI, "internal error: invalid rendering position!");
			break;
	}	
	g_free(tmp);
}

static void attribs_render_categories(gpointer data, struct displayset *displayset, gpointer user_data) {
	GSList	*categories = (GSList *)data;
	gchar	*escapedStr, *tmp = NULL;
	
	while(categories) {
		addToHTMLBufferFast(&tmp, (gchar *)categories->data);
		if(categories = g_slist_next(categories))
			addToHTMLBufferFast(&tmp, ",");
	}
	escapedStr = g_markup_escape_text(tmp, -1);
	FEED_FOOT_WRITE(displayset->foottable, _("categories"), escapedStr);
	g_free(escapedStr);
	g_free(tmp);
}

static void attribs_render_foot_text(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar	*tmp;

	tmp = g_strdup_printf("<div class='foottext'>%s</div>", (gchar *)data);
	addToHTMLBufferFast(&(displayset->foot), tmp);
	g_free(tmp);
}

static void attribs_render_comments_uri(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar *tmp;
	
	tmp = g_markup_printf_escaped("<div class='commentlink' style=\"margin-top:5px;margin-bottom:5px;\">(<a href=\"%s\">%s</a>)</div>", 
	                              (gchar*)data, _("comments"));
	
	addToHTMLBufferFast(&(displayset->foot), tmp);
	g_free(tmp);
}

static void attribs_render_enclosure(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar *tmp, *escapedLink, *filename; 
	
	filename = strrchr((char *)data, '/');
	if(filename != NULL)
		filename++; /* Skip the slash to find the filename */
	else
		filename = data;

	escapedLink = encode_uri_string(g_strdup((gchar *)data));
	tmp = g_markup_printf_escaped("<table class=\"enclosure\" cellpadding=\"0\" cellspacing=\"0\"><tr><td>"
	                              "<a href=\"liferea-enclosure://load?%s\">"
	                              "<img border=\"0\" class=\"enclosurebtn\" src=\"file://" 
		        	       PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S 
		        	       "pixmaps" G_DIR_SEPARATOR_S "arrow.png\"/></a></td><td>"
		        	       "&nbsp;<a class=\"enclosure\" href=\"%s\">%s</a>"
		        	       "</td></tr></table>", escapedLink, (gchar *)data, filename);
	addToHTMLBufferFast(&(displayset->head), tmp);
	g_free(escapedLink);
	g_free(tmp);
}

static void attribs_render_feedgenerator_uri(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar *tmp = g_markup_printf_escaped("<a href=\"%s\">%s</a>", (gchar*)data, (gchar*)data);
	FEED_FOOT_WRITE(displayset->foottable, _("feed generator"), tmp);
	g_free(tmp);
}

#define REGISTER_SIMPLE_ATTRIBUTE(position, strid, promptStr) do { \
	struct str_attrib *props = g_new(struct str_attrib, 1); \
	props->pos = (position); \
	props->prompt = (promptStr); \
	metadata_register_renderer(strid, attribs_render_str, FALSE, props); \
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
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "pubDate", _("feed published on"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "contentUpdateDate", _("content last updated"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "managingEditor", _("managing editor"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "webmaster", _("webmaster"));
	metadata_register_renderer("category",	attribs_render_categories, TRUE, NULL);
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "feedgenerator", _("feed generator"));
	metadata_register_renderer("imageUrl",	attribs_render_image, FALSE, GINT_TO_POINTER(POS_HEAD));
	metadata_register_renderer("textInput",	attribs_render_foot_text, FALSE, NULL);
	metadata_register_renderer("commentsUri", attribs_render_comments_uri, FALSE, NULL);
	metadata_register_renderer("enclosure",	attribs_render_enclosure, FALSE, NULL);
	
	/* types for admin */
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "errorReportsTo", _("report errors to"));
	metadata_register_renderer("feedgeneratorUri", attribs_render_feedgenerator_uri, FALSE, NULL);
	
	/* types for aggregation */
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "agSource", _("original source"));
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, "agTimestamp", _("original time"));

	/* types for blog channel */
	metadata_register_renderer("blogChannel", attribs_render_foot_text, FALSE, NULL);
	
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
	metadata_register_renderer("fmScreenshot", attribs_render_image, FALSE, GINT_TO_POINTER(POS_BODY));
	
	/* types for photo blogs */
	metadata_register_renderer("photo", ns_photo_render, FALSE, NULL);

	/* types for slash */
	metadata_register_renderer("slash", ns_slash_render, FALSE, NULL);	 /* This one should only be set, not appended */
}

static void attribs_register_default_renderer(const gchar *strid) {
	gchar *str = g_strdup(strid);
	
	REGISTER_SIMPLE_ATTRIBUTE(POS_FOOTTABLE, str, str);
}
