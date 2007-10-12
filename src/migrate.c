/**
 * @file migrate.c migration between different cache versions
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

#include <glib.h>
#include <libxml/uri.h>
#include "common.h"
#include "db.h"
#include "debug.h"
#include "feed.h"
#include "item.h"
#include "itemset.h"
#include "metadata.h"
#include "migrate.h"
#include "node.h"
#include "xml.h"

extern gboolean cacheMigrated;

static void 
migrate_copy_dir (const gchar *from,
                  const gchar *to,
                  const gchar *subdir) 
{
	gchar *fromDirname, *toDirname;
	gchar *srcfile, *destfile;
   	GDir *dir;
		
	fromDirname = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%s", g_get_home_dir(), from, subdir);
	toDirname = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%s", g_get_home_dir(), to, subdir);
	
	dir = g_dir_open(fromDirname, 0, NULL);
	while(NULL != (srcfile = (gchar *)g_dir_read_name(dir))) {
		gchar	*content;
		gsize	length;
		destfile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", toDirname, srcfile);
		srcfile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", fromDirname, srcfile);
		g_print("copying %s\n     to %s\n", srcfile, destfile);
		if(g_file_get_contents(srcfile, &content, &length, NULL))
			g_file_set_contents(destfile, content, length, NULL);
		g_free(content);
		g_free(destfile);
		g_free(srcfile);
	}
	g_dir_close(dir);
	
	g_free(fromDirname);
	g_free(toDirname);
}

static itemPtr
migrate_item_parse_cache (xmlNodePtr cur,
                          gboolean migrateCache) 
{
	itemPtr 	item;
	gchar		*tmp;
	
	g_assert(NULL != cur);
	
	item = item_new();
	item->popupStatus = FALSE;
	item->newStatus = FALSE;
	
	cur = cur->xmlChildrenNode;
	while(cur) {
		if(cur->type != XML_ELEMENT_NODE ||
		   !(tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
			cur = cur->next;
			continue;
		}
		
		if(!xmlStrcmp(cur->name, BAD_CAST"title"))
			item_set_title(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"description"))
			item_set_description(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"source"))
			item_set_source(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"real_source_url"))
			item_set_real_source_url(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"real_source_title"))
			item_set_real_source_title(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"id"))
			item_set_id(item, tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"validGuid"))
			item->validGuid = TRUE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"readStatus"))
			item->readStatus = (0 == atoi(tmp))?FALSE:TRUE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"updateStatus"))
			item->updateStatus = (0 == atoi(tmp))?FALSE:TRUE;

		else if(!xmlStrcmp(cur->name, BAD_CAST"mark")) 
			item->flagStatus = (1 == atoi(tmp))?TRUE:FALSE;
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"time"))
			item->time = atol(tmp);
			
		else if(!xmlStrcmp(cur->name, BAD_CAST"attributes"))
			item->metadata = metadata_parse_xml_nodes(cur);
		
		g_free(tmp);	
		tmp = NULL;
		cur = cur->next;
	}
	
	item->hasEnclosure = (NULL != metadata_list_get(item->metadata, "enclosure"));
	
	if (migrateCache && item->description) {
		gchar *desc = xhtml_from_text (item->description);
		item_set_description (item, desc);
		g_free(desc);
	}
	return item;
}

#define FEED_CACHE_VERSION       "1.1"

static void
migrate_load_from_cache (const gchar *sourceDir, const gchar *id) 
{
	nodePtr			node;
	feedParserCtxtPtr	ctxt;
	gboolean		migrateFrom10 = TRUE;
	gchar			*filename;
	guint 			itemCount = 0;
	
	debug_enter ("migrate_load_from_cache");

	node = node_from_id (id);
	if (!node) {
		debug1 (DEBUG_CACHE, "ignoring cache file %s because it is not referenced in feed list...", id);
		return;		/* propably a stale cache file */
	}

	ctxt = feed_create_parser_ctxt ();
	ctxt->subscription = node->subscription;
	ctxt->feed = (feedPtr)node->data;
		
	filename = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%s", sourceDir, node->id);
	debug2 (DEBUG_CACHE, "loading cache file \"%s\" (feed \"%s\")", filename, subscription_get_source(ctxt->subscription));
	
	if ((!g_file_get_contents (filename, &ctxt->data, &ctxt->dataLength, NULL)) || (*ctxt->data == 0)) {
		debug1 (DEBUG_CACHE, "could not load cache file %s", filename);
		g_free (filename);
		return;
	}

	g_print ("migrating feed %s ", id);

	do {
		xmlNodePtr cur;
		
		g_assert (NULL != ctxt->data);

		if (NULL == xml_parse_feed (ctxt))
			break;

		if (NULL == (cur = xmlDocGetRootElement (ctxt->doc)))
			break;

		while (cur && xmlIsBlankNode (cur))
			cur = cur->next;

		if (!xmlStrcmp (cur->name, BAD_CAST"feed")) {
			xmlChar *version;			
			if ((version = xmlGetProp (cur, BAD_CAST"version"))) {
				migrateFrom10 = xmlStrcmp (BAD_CAST FEED_CACHE_VERSION, version);
				xmlFree (version);
			}
		} else {
			break;		
		}

		cur = cur->xmlChildrenNode;
		while (cur) {

			if (!xmlStrcmp (cur->name, BAD_CAST"item")) {
				itemPtr item;
							
				itemCount++;
				item = migrate_item_parse_cache (cur, migrateFrom10);
				item->nodeId = g_strdup (id);
				
				/* migrate item to DB */
				g_assert (0 == item->id);
				db_item_update (item);
				
				if (0 == (itemCount % 10))
					g_print(".");
				item_unload(item);
			}
			
			cur = cur->next;
		}
	} while (FALSE);

	if (ctxt->doc)
		xmlFreeDoc (ctxt->doc);
		
	g_free (ctxt->data);
	g_free (filename);

	feed_free_parser_ctxt (ctxt);
	
	db_node_update (node);
	if (node->subscription)
		db_subscription_update (node->subscription);
	
	g_print ("\n");
	
	debug_exit ("migrate_load_from_cache");
}

static void
migrate_items (const gchar *sourceDir)
{
   	GDir 	*dir;
	gchar	*id;

	db_begin_transaction ();
		
	dir = g_dir_open (sourceDir, 0, NULL);
	while (NULL != (id = (gchar *)g_dir_read_name (dir))) 
	{
		debug_start_measurement (DEBUG_CACHE);
		migrate_load_from_cache (sourceDir, id);
		debug_end_measurement (DEBUG_CACHE, "parse feed");
	}
	g_dir_close (dir);
	
	db_commit_transaction ();
}

static void 
migrate_10_to_14 (void)
{
	gchar *sourceDir;
	
	g_print("Performing 1.0 -> 1.4 cache migration...\n");
	migrate_copy_dir (".liferea", ".liferea_1.4", "");
	migrate_copy_dir (".liferea", ".liferea_1.4", "cache" G_DIR_SEPARATOR_S "favicons");

	sourceDir = g_strdup_printf("%s" G_DIR_SEPARATOR_S ".liferea" G_DIR_SEPARATOR_S "cache" G_DIR_SEPARATOR_S "feeds", g_get_home_dir());	
	migrate_items(sourceDir);
	g_free(sourceDir);
}

static void
migrate_12_to_14 (void)
{
	gchar *sourceDir;
	
	g_print("Performing 1.2 -> 1.4 cache migration...\n");
	
	/* copy everything besides the feed cache */
	migrate_copy_dir (".liferea_1.2", ".liferea_1.4", "");
	migrate_copy_dir (".liferea_1.2", ".liferea_1.4", "cache" G_DIR_SEPARATOR_S "favicons");
	migrate_copy_dir (".liferea_1.2", ".liferea_1.4", "cache" G_DIR_SEPARATOR_S "scripts");
	migrate_copy_dir (".liferea_1.2", ".liferea_1.4", "cache" G_DIR_SEPARATOR_S "plugins");
	
	/* migrate feed cache to new DB format */
	sourceDir = g_strdup_printf("%s" G_DIR_SEPARATOR_S ".liferea_1.2" G_DIR_SEPARATOR_S "cache" G_DIR_SEPARATOR_S "feeds", g_get_home_dir());
	migrate_items(sourceDir);
	g_free(sourceDir);
}

static void
migrate_13_to_14 (void)
{
	g_print("Performing 1.3 -> 1.4 cache migration...\n");	
	
	/* close already loaded DB */
	db_deinit ();

	/* just copying all files */
	migrate_copy_dir (".liferea_1.3", ".liferea_1.4", "");
	migrate_copy_dir (".liferea_1.3", ".liferea_1.4", "cache" G_DIR_SEPARATOR_S "favicons");
	migrate_copy_dir (".liferea_1.3", ".liferea_1.4", "cache" G_DIR_SEPARATOR_S "scripts");
	migrate_copy_dir (".liferea_1.3", ".liferea_1.4", "cache" G_DIR_SEPARATOR_S "plugins");	
	
	/* and reopen the copied one */
	db_init (TRUE);
}

void
migration_execute (migrationMode mode)
{
	switch (mode) {
		case MIGRATION_MODE_10_TO_14:
			migrate_10_to_14 ();
			break;
		case MIGRATION_MODE_12_TO_14:
			migrate_12_to_14 ();
			break;
		case MIGRATION_MODE_13_TO_14:
			migrate_13_to_14 ();
			break;
		default:
			g_error ("Invalid migration mode!");
			return;
			break;
	}
	
	cacheMigrated = TRUE;
}
