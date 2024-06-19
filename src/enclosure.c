/*
 * @file enclosure.c enclosures/podcast support
 *
 * Copyright (C) 2007-2012 Lars Windolf <lars.windolf@gmx.de>
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
#include "enclosure.h"
#include "xml.h"
#include "ui/preferences_dialog.h"	// FIXME: remove this!
#include "ui/ui_common.h"

#if !defined (G_OS_WIN32) || defined (HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif

/*
   Liferea manages a MIME type configuration to allow
   comfortable enclosure/podcast handling that launches
   external applications to play or download content.
   
   The MIME type configuration is saved into a XML file
   in the cache directory.
   
   Enclosure download is currently done using external
   tools (gwget,kget,steadyflow...) and is performed by
   simply passing the URL. All downloads are asynchronous
   and download concurrency is considered to be handled
   by the invoked tools.
 */

static GSList *types = NULL;
static gboolean typesLoaded = FALSE;

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

static void
enclosure_mime_types_load (void)
{
	xmlDocPtr	doc;
	xmlNodePtr	cur;
	encTypePtr	etp;
	gchar		*filename;
	
	typesLoaded = TRUE;
	
	filename = common_create_config_filename ("mime.xml");
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		doc = xmlParseFile (filename);
		if (!doc) {
			debug (DEBUG_CONF, "could not load enclosure type config file!");
		} else {
			cur = xmlDocGetRootElement (doc);
			if (!cur) {
				g_warning ("could not read root element from enclosure type config file!");
			} else {
				while (cur) {
					if (!xmlIsBlankNode (cur)) {
						if (!xmlStrcmp (cur->name, BAD_CAST"types")) {
							cur = cur->xmlChildrenNode;
							while (cur) {
								if ((!xmlStrcmp (cur->name, BAD_CAST"type"))) {
									etp = g_new0 (struct encType, 1);
									etp->mime = (gchar *) xmlGetProp (cur, BAD_CAST"mime");
									etp->extension = (gchar *) xmlGetProp (cur, BAD_CAST"extension");
									etp->cmd = (gchar *) xmlGetProp (cur, BAD_CAST"cmd");
									etp->permanent = TRUE;
									types = g_slist_append (types, etp);
								}
								cur = cur->next;
							}
							break;
						} else {
							g_warning (_("\"%s\" is not a valid enclosure type config file!"), filename);
						}
					}
					cur = cur->next;
				}
			}
			xmlFreeDoc (doc);
		}
	}
	g_free (filename);
}

void
enclosure_mime_types_save (void)
{
	xmlDocPtr	doc;
	xmlNodePtr	root, cur;
	encTypePtr	etp;
	GSList		*iter;
	gchar		*filename;

	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewDocNode (doc, NULL, BAD_CAST"types", NULL);
	
	iter = types;
	while (iter) {
		etp = (encTypePtr)iter->data;
		cur = xmlNewChild (root, NULL, BAD_CAST"type", NULL);
		xmlNewProp (cur, BAD_CAST"cmd", BAD_CAST etp->cmd);
		if (etp->mime)
			xmlNewProp (cur, BAD_CAST"mime", BAD_CAST etp->mime);
		if (etp->extension)
			xmlNewProp (cur, BAD_CAST"extension", BAD_CAST etp->extension);
		iter = g_slist_next (iter);
	}
	
	xmlDocSetRootElement (doc, root);

	filename = common_create_config_filename ("mime.xml");
	if (-1 == xmlSaveFormatFileEnc (filename, doc, NULL, 1))
		g_warning ("Could not save to enclosure type config file!");
	g_free (filename);
	
	xmlFreeDoc (doc);
}

const GSList *
enclosure_mime_types_get (void)
{
	if (!typesLoaded)
		enclosure_mime_types_load ();
		
	return types;
}

void
enclosure_mime_type_add (encTypePtr type)
{
	types = g_slist_append (types, type);
	
	enclosure_mime_types_save ();
}

void 
enclosure_mime_type_remove (encTypePtr type)
{
	types = g_slist_remove (types, type);
	g_free (type->cmd);
	g_free (type->mime);
	g_free (type->extension);
	g_free (type);
	
	enclosure_mime_types_save ();
}

/* etp is optional, if it is missing we are in save mode */
void
enclosure_download (encTypePtr type, const gchar *url, gboolean interactive)
{
	GError	*error = NULL;
	gchar	*cmd, *urlQ;

	urlQ = g_shell_quote (url);
		
	if (type) {
		debug (DEBUG_UPDATE, "passing URL %s to command %s...", urlQ, type->cmd);
		cmd = g_strdup_printf ("%s %s", type->cmd, urlQ);
	} else {
		gchar *toolCmd = prefs_get_download_command ();
		if(!toolCmd) {
			if (interactive)
				ui_show_error_box (_("You have not configured a download tool yet! Please do so in the 'Enclosures' tab in Tools/Preferences."));
			return;
		}

		debug (DEBUG_UPDATE, "downloading URL %s with %s...", urlQ, toolCmd);
		cmd = g_strdup_printf (toolCmd, urlQ);
		g_free (toolCmd);
	}

	g_free (urlQ);

	/* free now unnecessary stuff */
	if (type && !type->permanent)
		enclosure_mime_type_remove (type);

	/* execute command */		
	g_spawn_command_line_async (cmd, &error);
	if (error && (0 != error->code)) {
		if (interactive)
			ui_show_error_box (_("Command failed: \n\n%s\n\n Please check whether the configured download tool is installed and working correctly! You can change it in the 'Download' tab in Tools/Preferences."), cmd);
		else
			g_warning ("Command \"%s\" failed!", cmd);
	}

	if (error)
		g_error_free (error);
	g_free (cmd);
}
