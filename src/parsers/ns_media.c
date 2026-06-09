/**
 * @file ns_media.c  Yahoo media namespace support / Media RSS Specification
 *
 * Copyright (C) 2019 Mikel Olasagasti Uranga <mikel@olasagasti.info>
 * Copyright (C) 2007-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include <string.h>

#include "ns_media.h"

#include "enclosure.h"
#include "metadata.h"
#include "xml.h"

/* a namespace documentation can be found at 
   http://www.rssboard.org/media-rss
*/

static void
parse_item_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	gchar *description, *tmp, *tmp2, *thumbnail, *count, *max, *avg, *views;
	/*
	   Maximum definition could look like this:
	   
        	<media:content 
        	       url="http://www.foo.com/movie.mov" 
        	       fileSize="12216320" 
        	       type="video/quicktime"
        	       medium="video"
        	       isDefault="true" 
        	       expression="full" 
        	       bitrate="128" 
        	       framerate="25"
        	       samplingrate="44.1"
        	       channels="2"
        	       duration="185" 
        	       height="200"
        	       width="300" 
        	       lang="en" />
		       
	   (example quoted from specification https://www.rssboard.org/media-rss#media-content)
	*/
	if (!xmlStrcmp(cur->name, BAD_CAST"content")) {
		tmp = xml_get_attribute (cur, "url");
		if (!tmp)
			return;

		/* the following code is duplicated from rss_item.c! */
		const gchar *feedURL = subscription_get_homepage (ctxt->subscription);
	
		g_autofree gchar *type = xml_get_attribute (cur, "type");
		g_autofree gchar *medium = xml_get_attribute (cur, "medium");
		g_autofree gchar *fileSize = xml_get_attribute (cur, "fileSize");
		g_autofree gchar *width = xml_get_attribute (cur, "width");
		g_autofree gchar *height = xml_get_attribute (cur, "height");
		gssize l = -1, h = -1, w = -1;

		if (fileSize)
			l = atol (fileSize);
		if (width)
			w = atol (width);
		if (height)
			h = atol (height);

		 /* if the URL is relative and we have a feed URL, try to add it */
	
		if ((strstr (tmp, "://") == NULL) && feedURL && (feedURL[0] != '|') &&
		    (strstr (feedURL, "://") != NULL)) {
			/* add base URL if necessary and possible */
			 tmp2 = g_strdup_printf ("%s/%s", feedURL, tmp);
			 g_free (tmp);
			 tmp = tmp2;
		}
	
		/* gravatars are often supplied as media:content with medium='image'
		   so we do not treat such occurences as enclosures */
		if (medium && !strcmp (medium, "image") && strstr (tmp, "www.gravatar.com")) {
			metadata_list_set (&(ctxt->item->metadata), "gravatar", tmp);
		} else {
			/* Never add enclosures for images already contained in the description */
			if (!(ctxt->item->description && strstr (ctxt->item->description, tmp))) {
				item_add_enclosure (ctxt->item, enclosure_new (tmp, type, l, w, h));
			}
		}
		g_free (tmp);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST"group")) {
		cur = cur->xmlChildrenNode;
		while (cur) {
			if (cur->type == XML_ELEMENT_NODE)
				parse_item_tag (ctxt, cur);
			cur = cur->next;
		}
	}
	else if (!xmlStrcmp(cur->name, BAD_CAST"thumbnail")) {
		thumbnail = xml_get_attribute (cur, "url");
		if (!thumbnail)
			return;

		metadata_list_set (&(ctxt->item->metadata), "mediathumbnail", thumbnail);
		g_free (thumbnail);

	}
	else if (!xmlStrcmp(cur->name, BAD_CAST"description")) {
		description = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);

		if (!description)
			return;

		metadata_list_set (&(ctxt->item->metadata), "mediadescription", description);
		g_free (description);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST"community")) {
		cur = cur->xmlChildrenNode;
		while (cur) {
			if (cur->type == XML_ELEMENT_NODE)
				parse_item_tag (ctxt, cur);
			cur = cur->next;
		}

	}
	else if (!xmlStrcmp(cur->name, BAD_CAST"starRating")) {
			count = xml_get_attribute (cur, "count");
			avg = xml_get_attribute (cur, "average");
			max = xml_get_attribute (cur, "max");
			if (!avg)
				return;
			metadata_list_set (&(ctxt->item->metadata), "mediastarRatingcount", count);
			metadata_list_set (&(ctxt->item->metadata), "mediastarRatingavg", avg);
			metadata_list_set (&(ctxt->item->metadata), "mediastarRatingmax", max);
			g_free (count);
			g_free (avg);
			g_free (max);
	}
	else if (!xmlStrcmp(cur->name, BAD_CAST"statistics")) {
			views = xml_get_attribute (cur, "views");

			if (!views)
				return;
			metadata_list_set (&(ctxt->item->metadata), "mediaviews", views);
			g_free (views);
	}
	// FIXME: should we support media:player too?
}

void
ns_media_register_ns (GHashTable *prefixhash, GHashTable *urihash)
{
	static NsHandler nsh = {
		.parseItemTag = parse_item_tag,
	};
	g_hash_table_insert (prefixhash, "media", &nsh);
	g_hash_table_insert (urihash, "http://www.rssboard.org/media-rss", &nsh);
}