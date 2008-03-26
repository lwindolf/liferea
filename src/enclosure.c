/**
 * @file enclosure.c enclosures/podcast support
 *
 * Copyright (C) 2007-2008 Lars Lindner <lars.lindner@gmail.com>
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

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "enclosure.h"
#include "xml.h"
#include "ui/ui_prefs.h"	// FIXME: remove this!

/*
   Liferea manages a MIME type configuration to allow
   comfortable enclosure/podcast handling that launches
   external applications to play/display the downloaded
   content.
   
   The MIME type configuration is saved into a XML file
   in the cache directory.
   
   Enclosure download is currently done using external
   tools (wget,curl,gwget...) and is performed by a new
   glib thread that first downloads the file and then 
   starts the configured launcher command in background
   and terminates.
   
   There is also an automatic enclosure downloading 
   feature that just downloads enclosures but does not
   trigger any launcher command.
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
	
	fields = g_regex_split_simple ("^enc:([01]?):([^:]+):(\\d+):(.*)", str, 0, 0);
	if (6 > g_strv_length (fields)) {
		g_warning ("Dropping incorrectly encoded enclosure: >>>%s<<< (nr of fields=%d)\n", str, g_strv_length (fields));
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
enclosure_values_to_string (const gchar *url, const gchar *mime, gsize size, gboolean downloaded)
{
	return g_strdup_printf ("enc:%s:%s:%d:%s", downloaded?"1":"0", mime, size, url);
}

gchar *
enclosure_to_string (enclosurePtr enclosure)
{
	return enclosure_values_to_string (enclosure->url,
	                                   enclosure->mime,
	                                   enclosure->size,
	                                   enclosure->downloaded);
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
	
	filename = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "mime.xml", common_get_cache_path());
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		doc = xmlParseFile (filename);
		if (!doc) {
			debug0 (DEBUG_CONF, "could not load enclosure type config file!");
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
									etp->mime = xmlGetProp (cur, BAD_CAST"mime");
									etp->extension = xmlGetProp (cur, BAD_CAST"extension");
									etp->cmd = xmlGetProp (cur, BAD_CAST"cmd");
									etp->remote = xmlStrcmp(BAD_CAST"false", xmlGetProp(cur, BAD_CAST"remote"));
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

static void
enclosure_mime_types_save (void)
{
	xmlDocPtr	doc;
	xmlNodePtr	root, cur;
	encTypePtr	etp;
	GSList		*iter;
	gchar		*filename;

	doc = xmlNewDoc ("1.0");	
	root = xmlNewDocNode (doc, NULL, BAD_CAST"types", NULL);
	
	iter = types;
	while (iter) {
		etp = (encTypePtr)iter->data;
		cur = xmlNewChild (root, NULL, BAD_CAST"type", NULL);
		xmlNewProp (cur, BAD_CAST"cmd", etp->cmd);
		xmlNewProp (cur, BAD_CAST"remote", etp->remote?"true":"false");
		if (etp->mime)
			xmlNewProp (cur, BAD_CAST"mime", etp->mime);
		if (etp->extension)
			xmlNewProp (cur, BAD_CAST"extension", etp->extension);
		iter = g_slist_next (iter);
	}
	
	xmlDocSetRootElement (doc, root);
	
	filename = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "mime.xml", common_get_cache_path ());
	if (-1 == xmlSaveFormatFileEnc (filename, doc, NULL, 1))
		g_warning ("Could not save to enclosure type config file!");
	g_free (filename);
	
	xmlFreeDoc (doc);
}

const GSList const *
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

/** enclosure downloading/launching job parameters */
typedef struct encJob {
	gchar	*download;	/**< (optional) command to download */
	gchar	*run;		/**< command to run the downloaded file or the URL with */
	gchar	*filename;	/**< filename the result is saved to */
} *encJobPtr;

static gpointer
enclosure_exec (gpointer data)
{
	encJobPtr	ejp = (encJobPtr)data;
	GError		*error = NULL;
	gint		status;
	
	/* Download is optional when just passing URLs */
	if (ejp->download) {
		debug1 (DEBUG_UPDATE, "running download command \"%s\"", ejp->download);
		g_spawn_command_line_sync (ejp->download, NULL, NULL, &status, &error);
	}
	
	if (error && (0 != error->code)) {
		g_warning ("Download command \"%s\" failed with exitcode %d!", ejp->download, status);
	} else {
		if (ejp->run) {
			/* execute */
			debug1 (DEBUG_UPDATE, "running launch command \"%s\"", ejp->run);
			g_spawn_command_line_async (ejp->run, &error);
			if (error && (0 != error->code))
				g_warning ("Launch command \"%s\" failed!", ejp->run);
		} else {
			/* just saving */
			ui_mainwindow_set_status_bar (_("Enclosure download finished: \"%s\""), ejp->filename);
		}
	}
	g_free (ejp->download);
	g_free (ejp->run);
	g_free (ejp->filename);
	g_free (ejp);

	return NULL;
}

/* etp is optional, if it is missing we are in save mode */
void
enclosure_download (encTypePtr type, const gchar *url, const gchar *filename)
{
	enclosureDownloadToolPtr 	tool;
	encJobPtr			job;
	gchar 				*filenameQ, *urlQ;

	/* prepare job structure */
	job = g_new0 (struct encJob, 1);
	job->filename = g_strdup (filename);

	filenameQ = g_shell_quote (filename);
	urlQ = g_shell_quote (url);
	
	tool = prefs_get_download_tool ();
	if (tool->niceFilename)
		job->download = g_strdup_printf (tool->format, filenameQ, urlQ);
	else
		job->download = g_strdup_printf (tool->format, urlQ);
		
	if (type) {
	
		/* Argh... If the remote flag is set we do not want to download
		   the enclosure ourselves but just want to pass the URL
		   to the configured command */
		   	
		if (type->remote) {
			g_free (job->download);
			job->download = NULL;
		}
		
		if (type->remote)
			job->run = g_strdup_printf ("%s %s", type->cmd, urlQ);
		else
			job->run = g_strdup_printf ("%s %s", type->cmd, filenameQ);
	}

	g_free (filenameQ);
	g_free (urlQ);
	
	if (type && type->remote)
		debug1 (DEBUG_UPDATE, "passing URL %s to command...", url);
	else
		debug2 (DEBUG_UPDATE, "downloading %s to %s...", url, filename);

	/* free now unnecessary stuff */
	if (type && !type->permanent)
		enclosure_mime_type_remove (type);
	
	g_thread_create (enclosure_exec, job, FALSE, NULL);
}

/**
 * Download an enclosure at "url" and save it to "filename". If
 * filename is NULL, then a filename will be automatically generated
 * based on the URL.
 */
void
enclosure_save_as_file (encTypePtr type, const gchar *url, const gchar *filename)
{	
	g_assert (url != NULL);
	
	if (!filename) {
		/* build filename from last part of URL and make it begin with
		   the default enclosure save path */
		filename = strrchr(url, '/');
		if (!filename)
			filename = url;
		else
			filename++;
		filename = g_strdup_printf ("%s%s%s", conf_get_str_value (ENCLOSURE_DOWNLOAD_PATH), G_DIR_SEPARATOR_S, filename);
	}
	enclosure_download (type, url, filename);
}
