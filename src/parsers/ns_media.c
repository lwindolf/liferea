/**
 * @file ns_media.c Yahoo media namespace support
 *
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <string.h>

#include "ns_media.h"
#include "common.h"
#include "xml.h"

/* a namespace documentation can be found at 
   http://search.yahoo.com/mrss   
*/

static void
parse_item_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	gchar *tmp, *tmp2;
	/*
	   Maximual definition could look like this:
	   
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
		       
	   (example quoted from specification)
	*/
  	if (!xmlStrcmp(cur->name, "content")) {
		if (tmp = common_utf8_fix (xmlGetProp (cur, BAD_CAST"url"))) {
			/* the following code is duplicated from rss_item.c! */
			if ((strstr (tmp, "://") == NULL) &&
			    (ctxt->feed->htmlUrl != NULL) &&
			    (ctxt->feed->htmlUrl[0] != '|') &&
			    (strstr (ctxt->feed->htmlUrl, "://") != NULL)) {
				/* add base URL if necessary and possible */
				 tmp2 = g_strdup_printf ("%s/%s", ctxt->feed->htmlUrl, tmp);
				 g_free (tmp);
				 tmp = tmp2;
			}

			ctxt->item->metadata = metadata_list_append (ctxt->item->metadata, "enclosure", tmp);
			ctxt->item->hasEnclosure = TRUE;
			g_free (tmp);
		}
	}
	
	// FIXME: should we support media:player too?
}

static void
ns_media_register_ns (NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash)
{
	g_hash_table_insert (prefixhash, "media", nsh);
	g_hash_table_insert (urihash, "http://search.yahoo.com/mrss", nsh);
}

NsHandler *
ns_media_get_handler (void)
{
	NsHandler 	*nsh;
	
	nsh = g_new0 (NsHandler, 1);
	nsh->prefix		= "media";
	nsh->registerNs		= ns_media_register_ns;
	nsh->parseItemTag	= parse_item_tag;

	return nsh;
}
