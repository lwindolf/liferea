/*
 * @file enclosure.c enclosures/podcast support
 *
 * Copyright (C) 2007-2024 Lars Windolf <lars.windolf@gmx.de>
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
#include "xml.h"
#include "ui/ui_common.h"

#if !defined (G_OS_WIN32) || defined (HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif

/* The internal enclosure encoding format is either

      <url>
      
   or 
   
      enc:<downloaded flag>:<mime type>:<length in byte>:<url>
      
   Examples:
   
      "http://somewhere.com/cool.mp3"
      "enc::::http://somewhere.com/cool.mp3"
      "enc:0:audio/ogg:237423414:http://somewhere.com/cool.ogg"
      "enc:1:x-application/pdf::https://secret.site.us/defense-plan.pdf"      
  */
      
enclosurePtr
enclosure_from_string (const gchar *str)
{
	gchar 		**fields;
	enclosurePtr	enclosure;
	
	enclosure = g_new0 (struct enclosure, 1);
	
	/* legacy URL, migration case... */
	if (strstr (str, "enc:") != str) {
		enclosure->url = g_strdup (str);
		return enclosure;
	}
	
	fields = g_regex_split_simple ("^enc:([01]?):([^:]*):(\\d+):(.*)", str, 0, 0);
	if (6 > g_strv_length (fields)) {
		debug (DEBUG_PARSING, "Dropping incorrectly encoded enclosure: >>>%s<<< (nr of fields=%d)", str, g_strv_length (fields));
		enclosure_free (enclosure);
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

gchar *
enclosure_values_to_string (const gchar *url, const gchar *mime, gssize size, gboolean downloaded)
{
	gchar *result, *safeUrl;
	
	/* There are websites out there encoding -1 as size */
	if (size < 0)
		size = 0;
		
	safeUrl = (gchar *) common_uri_escape (BAD_CAST url);
	result = g_strdup_printf ("enc:%s:%s:%" G_GSSIZE_FORMAT ":%s", downloaded?"1":"0", mime?mime:"", size, safeUrl);
	g_free (safeUrl);
	
	return result;
}

gchar *
enclosure_to_string (enclosurePtr enclosure)
{
	return enclosure_values_to_string (enclosure->url,
	                                   enclosure->mime,
	                                   enclosure->size,
	                                   enclosure->downloaded);
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
	gchar		*mime = NULL;

	if (enclosure) {
		mime = g_strdup (enclosure->mime);
		enclosure_free (enclosure);
	}

	return mime;
}

void
enclosure_free (enclosurePtr enclosure)
{
	g_free (enclosure->url);
	g_free (enclosure->mime);
	g_free (enclosure);
}