/*
 * @file enclosure.c enclosures/podcast support
 *
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

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "download.h"
#include "enclosure.h"
#include "ui/ui_common.h"

#if !defined (G_OS_WIN32) || defined (HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif

/* The internal enclosure encoding format is either

      <url>
      
   or 
   
      enc:<downloaded flag>:<mime type>:<length in byte>:<url>

   or JSON with the following fields:
   
      {
	"downloaded": true|false,
	"mime": "<mime type>",
	"size": <size in byte>,
	"url": "<url>",
	"height": <height in pixel>,         # optional
	"width": <width in pixel>            # optional
      }
      
   Examples:
   
      "http://somewhere.com/cool.mp3"
      "enc::::http://somewhere.com/cool.mp3"
      "enc:0:audio/ogg:237423414:http://somewhere.com/cool.ogg"
      "enc:1:x-application/pdf::https://secret.site.us/defense-plan.pdf"      
  */

enclosurePtr
enclosure_from_json (const gchar *json_str)
{
	enclosurePtr enclosure = g_new0 (struct enclosure, 1);
	g_autoptr(JsonParser) parser = json_parser_new ();
	gboolean valid;

	// -1 means not set
	enclosure->size = -1;
	enclosure->width = -1;
	enclosure->height = -1;

	valid = json_parser_load_from_data (parser, json_str, -1, NULL);
	if (!valid) {
		debug (DEBUG_PARSING, "Ignoring incorrectly encoded enclosure: >>>%s<<< (not a valid JSON)", json_str);
		enclosure_free (enclosure);
		return NULL;
	}

	JsonNode *root = json_parser_get_root (parser);
	if (!JSON_NODE_HOLDS_OBJECT (root)) {
		debug (DEBUG_PARSING, "Ignoring incorrectly encoded enclosure: >>>%s<<< (not a JSON object)", json_str);
		enclosure_free (enclosure);
		return NULL;
	}
	JsonObject *obj = json_node_get_object (root);
	if (json_object_has_member (obj, "downloaded"))
		enclosure->downloaded = json_object_get_boolean_member (obj, "downloaded");
	if (json_object_has_member (obj, "mime"))
		enclosure->mime = g_strdup (json_object_get_string_member (obj, "mime"));
	if (json_object_has_member (obj, "size"))
		enclosure->size = json_object_get_int_member (obj, "size");
	if (json_object_has_member (obj, "width"))
		enclosure->width = json_object_get_int_member (obj, "width");
	if (json_object_has_member (obj, "height"))
		enclosure->height = json_object_get_int_member (obj, "height");
	if (json_object_has_member (obj, "url"))
		enclosure->url = g_strdup (json_object_get_string_member (obj, "url"));
	else {
		debug (DEBUG_PARSING, "Ignoring incorrectly encoded enclosure: >>>%s<<< (missing URL)", json_str);
		enclosure_free (enclosure);
		return NULL;
	}
	return enclosure;
}

enclosurePtr
enclosure_from_string (const gchar *str)
{
	gchar 		**fields;
	enclosurePtr	enclosure;

	g_assert_nonnull (str);

	if (str[0] == '{')
		return enclosure_from_json (str);
	
	enclosure = g_new0 (struct enclosure, 1);
	
	/* legacy URL, migration case... */
	if (strstr (str, "enc:") != str) {
		enclosure->url = g_strdup (str);
		return enclosure;
	}
	
	/* legacy "enc:" parsing */
	fields = g_regex_split_simple ("^enc:([01]?):([^:]*):(\\d+):(.*)", str, 0, 0);
	if (6 > g_strv_length (fields)) {
		debug (DEBUG_PARSING, "Dropping incorrectly encoded enclosure: >>>%s<<< (nr of fields=%d)", str, g_strv_length (fields));
		enclosure_free (enclosure);
		g_strfreev (fields);
		return NULL;
	}
	
	enclosure->downloaded = ('1' == *fields[1]);
	if (strlen (fields[2]))
		enclosure->mime = g_strdup (fields[2]);
	if (strlen (fields[3]))
		enclosure->size = atol (fields[3]);
	enclosure->url = g_strdup (fields[4]);

	g_strfreev (fields);

	return enclosure;
}

void
enclosure_to_json (enclosurePtr enclosure, JsonBuilder *b)
{
	g_autofree gchar *safeUrl;

	safeUrl = (gchar *) common_uri_escape (BAD_CAST enclosure->url);

	json_builder_begin_object (b);

	if (enclosure->downloaded) {
		json_builder_set_member_name (b, "downloaded");
		json_builder_add_boolean_value (b, enclosure->downloaded);
	}
	if (enclosure->mime) {
		json_builder_set_member_name (b, "mime");
		json_builder_add_string_value (b, enclosure->mime);
	}
	if (enclosure->size > 0) {
		json_builder_set_member_name (b, "size");
		json_builder_add_int_value (b, enclosure->size);
	}
	if (enclosure->width > 0) {
		json_builder_set_member_name (b, "width");
		json_builder_add_int_value (b, enclosure->width);
	}
	if (enclosure->height > 0) {
		json_builder_set_member_name (b, "height");
		json_builder_add_int_value (b, enclosure->height);
	}
	json_builder_set_member_name (b, "url");
	json_builder_add_string_value (b, safeUrl);
	json_builder_end_object (b);
}

gchar *
enclosure_to_string (enclosurePtr enclosure)
{
	g_autoptr(JsonBuilder) b = json_builder_new ();
	enclosure_to_json (enclosure, b);
	return json_dump (b);
}

gchar *
enclosure_get_url (const gchar *str)
{
	enclosurePtr enclosure = enclosure_from_string(str);
	gchar *url = NULL;

	if (enclosure) {
		url = g_strdup (enclosure->url);
		enclosure_free (enclosure);
	}

	return url;
}

gchar *
enclosure_get_mime (const gchar *str)
{
	enclosurePtr	enclosure = enclosure_from_string (str);
	gchar		*mime;

	if (!enclosure)
		return NULL;

	mime = g_strdup (enclosure->mime);
	enclosure_free (enclosure);

	return mime;
}

enclosurePtr
enclosure_new (const gchar *url, const gchar *mime, gssize length, gssize width, gssize height)
{
	enclosurePtr enclosure = g_new0 (struct enclosure, 1);

	enclosure->url = g_strdup (url);
	enclosure->mime = g_strdup (mime);
	enclosure->size = length;
	enclosure->width = width;
	enclosure->height = height;

	return enclosure;
}

void
enclosure_free (enclosurePtr enclosure)
{
	g_free (enclosure->url);
	g_free (enclosure->mime);
	g_free (enclosure);
}