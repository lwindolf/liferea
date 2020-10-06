/**
 * @file metadata.c  handling of typed item and feed meta data
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2014 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2015 Rich Coe <rcoe@wi.rr.com>
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
#include <string.h>

#include "common.h"
#include "debug.h"
#include "metadata.h"
#include "xml.h"

/* Metadata in Liferea are ordered lists of key/value list pairs. Both
   feed list nodes and items can have a list of metadata assigned. Metadata
   date values are always text values but maybe of different type depending
   on their usage type. */

static GHashTable *metadataTypes = NULL;	/**< hash table with all registered meta data types */

struct pair {
	gchar		*strid;		/** metadata type id */
	GSList		*data;		/** list of metadata values */
};

/* register metadata types to check validity on adding */
static void
metadata_init (void)
{
	g_assert (NULL == metadataTypes);

	metadataTypes = g_hash_table_new (g_str_hash, g_str_equal);

	/* generic types */
	metadata_type_register ("author",		METADATA_TYPE_HTML);
	metadata_type_register ("contributor",		METADATA_TYPE_HTML);
	metadata_type_register ("copyright",		METADATA_TYPE_HTML);
	metadata_type_register ("language",		METADATA_TYPE_HTML);
	metadata_type_register ("pubDate",		METADATA_TYPE_TEXT);
	metadata_type_register ("contentUpdateDate",	METADATA_TYPE_TEXT);
	metadata_type_register ("managingEditor",	METADATA_TYPE_HTML);
	metadata_type_register ("webmaster",		METADATA_TYPE_HTML);
	metadata_type_register ("feedgenerator",	METADATA_TYPE_HTML);
	metadata_type_register ("imageUrl",		METADATA_TYPE_URL);
	metadata_type_register ("icon",			METADATA_TYPE_URL);
	metadata_type_register ("homepage",		METADATA_TYPE_URL);
	metadata_type_register ("textInput",		METADATA_TYPE_HTML);
	metadata_type_register ("errorReportsTo",	METADATA_TYPE_HTML);
	metadata_type_register ("feedgeneratorUri",	METADATA_TYPE_URL);
	metadata_type_register ("category",		METADATA_TYPE_HTML);
	metadata_type_register ("enclosure",		METADATA_TYPE_TEXT);
	metadata_type_register ("commentsUri",		METADATA_TYPE_URL);
	metadata_type_register ("commentFeedUri",	METADATA_TYPE_URL);
	metadata_type_register ("feedTitle",		METADATA_TYPE_HTML);
	metadata_type_register ("description",		METADATA_TYPE_HTML);
	metadata_type_register ("richContent",		METADATA_TYPE_HTML5);

	/* types for aggregation NS */
	metadata_type_register ("agSource",		METADATA_TYPE_URL);
	metadata_type_register ("agTimestamp",		METADATA_TYPE_TEXT);

	/* types for blog channel */
	metadata_type_register ("blogChannel",		METADATA_TYPE_HTML);

	/* types for creative commons */
	metadata_type_register ("license",		METADATA_TYPE_HTML);

	/* types for Dublin Core (some dc tags are mapped to feed and item metadata types) */
	metadata_type_register ("creator",		METADATA_TYPE_HTML);
	metadata_type_register ("publisher",		METADATA_TYPE_HTML);
	metadata_type_register ("type",			METADATA_TYPE_HTML);
	metadata_type_register ("format",		METADATA_TYPE_HTML);
	metadata_type_register ("identifier",		METADATA_TYPE_HTML);
	metadata_type_register ("source",		METADATA_TYPE_URL);
	metadata_type_register ("coverage",		METADATA_TYPE_HTML);

	/* types for photo blogs */
	metadata_type_register ("photo", 		METADATA_TYPE_URL);

	/* types for slash */
	metadata_type_register ("slash",		METADATA_TYPE_HTML);

	/* type for gravatars */
	metadata_type_register ("gravatar",		METADATA_TYPE_URL);

	/* for RSS 2.0 real source and newsbin real source info */
	metadata_type_register ("realSourceUrl",	METADATA_TYPE_URL);
	metadata_type_register ("realSourceTitle",	METADATA_TYPE_URL);

	/* for trackback URL */
	metadata_type_register ("related",		METADATA_TYPE_URL);
	metadata_type_register ("via",                  METADATA_TYPE_URL);

	/* for georss:point */
	metadata_type_register ("point", 		METADATA_TYPE_TEXT);

	/* for mediaRSS */
	metadata_type_register ("mediadescription", 	METADATA_TYPE_HTML);
	metadata_type_register ("mediathumbnail", 	METADATA_TYPE_URL);
	metadata_type_register ("mediastarRatingcount", METADATA_TYPE_TEXT);
	metadata_type_register ("mediastarRatingavg", 	METADATA_TYPE_TEXT);
	metadata_type_register ("mediastarRatingmax", 	METADATA_TYPE_TEXT);
	metadata_type_register ("mediaviews", 		METADATA_TYPE_TEXT);

	return;
}

void
metadata_type_register (const gchar *name, gint type)
{
	if (!metadataTypes)
		metadata_init ();

	g_hash_table_insert (metadataTypes, (gpointer)name, GINT_TO_POINTER (type));
}

gboolean
metadata_is_type_registered (const gchar *strid)
{
	if (!metadataTypes)
		metadata_init ();

	if (g_hash_table_lookup (metadataTypes, strid))
		return TRUE;
	else
		return FALSE;
}

static gint
metadata_get_type (const gchar *name)
{
	gint	type;

	if (!metadataTypes)
		metadata_init ();

	type = GPOINTER_TO_INT (g_hash_table_lookup (metadataTypes, (gpointer)name));
	if (0 == type)
		g_warning ("Unknown metadata type: %s, please report this Liferea bug!", name);

	return type;
}

static gint
metadata_value_cmp (gconstpointer a, gconstpointer b)
{
	if (g_str_equal ((gchar *)a, (gchar *)b))
		return 0;

	return 1;
}

GSList *
metadata_list_append (GSList *metadata, const gchar *strid, const gchar *data)
{
	GSList		*iter = metadata;
	gchar		*tmp, *checked_data = NULL;
	struct pair 	*p;

	if (!data)
		return metadata;

	/* lookup type and check format */
	switch (metadata_get_type (strid)) {
		case METADATA_TYPE_TEXT:
			/* No check because renderer will process further */
			checked_data = g_strdup (data);
			break;
		case METADATA_TYPE_URL:
			/* Simple sanity check to see if it doesn't break XML */
			if (!strchr(data, '<') && !(strchr (data, '>')) && !(strchr (data, '&'))) {
				checked_data = g_strdup (data);
			} else {
				checked_data = common_uri_escape (data);
			}

			/* finally strip whitespace */
			checked_data = g_strchomp (checked_data);
			break;
		default:
			g_warning ("Unknown metadata type: %s (id=%d), please report this Liferea bug! Treating as HTML.", strid, metadata_get_type (strid));
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
		case METADATA_TYPE_HTML5:
			/* Remove DHTML */
			checked_data = g_strdup (data);
			break;
	}

	while (iter) {
		p = (struct pair*)iter->data;
		if (g_str_equal (p->strid, strid)) {
			/* Avoid duplicate values */
			if (NULL == g_slist_find_custom (p->data, checked_data, metadata_value_cmp))
				p->data = g_slist_append (p->data, checked_data);
                        else
                                g_free (checked_data);
			return metadata;
		}
		iter = iter->next;
	}
	p = g_new (struct pair, 1);
	p->strid = g_strdup (strid);
	p->data = g_slist_append (NULL, checked_data);
	metadata = g_slist_append (metadata, p);
	return metadata;
}

void
metadata_list_set (GSList **metadata, const gchar *strid, const gchar *data)
{
	GSList	*iter = *metadata;
	struct pair *p;

	while (iter) {
		p = (struct pair*)iter->data;
		if (g_str_equal (p->strid, strid)) {
                        g_slist_free_full (p->data, g_free);
                        p->data = g_slist_append (NULL, g_strdup (data));
			return;
		}
		iter = iter->next;
	}
	p = g_new (struct pair, 1);
	p->strid = g_strdup (strid);
	p->data = g_slist_append (NULL, g_strdup (data));
	*metadata = g_slist_append (*metadata, p);
}

void
metadata_list_foreach (GSList *metadata, metadataForeachFunc func, gpointer user_data)
{
	GSList	*list = metadata;
	guint	index = 0;

	while (list) {
		struct pair *p = (struct pair*)list->data;
		GSList *values = (GSList *)p->data;
		while (values) {
			index++;
			(*func)(p->strid, values->data, index, user_data);
			values = g_slist_next (values);
		}
		list = list->next;
	}
}

GSList *
metadata_list_get_values (GSList *metadata, const gchar *strid)
{
	GSList *list = metadata;

	while (list) {
		struct pair *p = (struct pair*)list->data;
		if (g_str_equal (p->strid, strid))
			return p->data;
		list = list->next;
	}
	return NULL;
}

const gchar *
metadata_list_get (GSList *metadata, const gchar *strid)
{
	GSList	*values;

	values = metadata_list_get_values (metadata, strid);
	return values?values->data:NULL;

}

GSList *
metadata_list_copy (GSList *list)
{
	GSList		*copy = NULL;
	GSList		*iter2, *iter = list;
	struct pair	*p;

	while (iter) {
		p = (struct pair*)iter->data;
		iter2 = p->data;
		while (iter2) {
			copy = metadata_list_append (copy, p->strid, iter2->data);
			iter2 = iter2->next;
		}
		iter = iter->next;
	}

	return copy;
}

void
metadata_list_free (GSList *metadata)
{
	GSList *iter = metadata;

	while (iter) {
		struct pair *p = (struct pair*)iter->data;
                g_slist_free_full (p->data, g_free);
		g_free (p->strid);
		g_free (p);
		iter = iter->next;
	}
	g_slist_free (metadata);
}

void
metadata_add_xml_nodes (GSList *metadata, xmlNodePtr parentNode)
{
	GSList *list = metadata;
	xmlNodePtr attribute;
	xmlNodePtr metadataNode = xmlNewChild (parentNode, NULL, "attributes", NULL);

	while (list) {
		struct pair *p = (struct pair*)list->data;
		GSList *list2 = p->data;
		while (list2) {
			attribute = xmlNewTextChild (metadataNode, NULL, "attribute", list2->data);
			xmlNewProp (attribute, "name", p->strid);
			list2 = list2->next;
		}
		list = list->next;
	}
}
