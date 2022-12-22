/**
 * @file export.c  OPML feed list import & export
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "export.h"

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libxml/tree.h>

#include "auth.h"
#include "common.h"
#include "db.h"
#include "debug.h"
#include "favicon.h"
#include "feedlist.h"
#include "folder.h"
#include "node.h"
#include "xml.h"
#include "ui/ui_common.h"
#include "ui/feed_list_view.h"

struct exportData {
	gboolean	trusted; /**< Include all the extra Liferea-specific tags */
	xmlNodePtr	cur;
};

static void export_node_children (nodePtr node, xmlNodePtr cur, gboolean trusted);

/* Used for exporting, this adds a folder or feed's node to the XML tree */
static void
export_append_node_tag (nodePtr node, gpointer userdata)
{
	xmlNodePtr 	cur = ((struct exportData*)userdata)->cur;
	gboolean	internal = ((struct exportData*)userdata)->trusted;
	xmlNodePtr	childNode;

	/* When exporting external OPML do not export every node type... */
	if (!(internal || (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_EXPORT)))
		return;

	childNode = xmlNewChild (cur, NULL, BAD_CAST"outline", NULL);

	/* 1. write generic node attributes */
	xmlNewProp (childNode, BAD_CAST"title", BAD_CAST node_get_title(node));
	xmlNewProp (childNode, BAD_CAST"text", BAD_CAST node_get_title(node)); /* The OPML spec requires "text" */
	xmlNewProp (childNode, BAD_CAST"description", BAD_CAST node_get_title(node));

	if (node_type_to_str (node))
		xmlNewProp (childNode, BAD_CAST"type", BAD_CAST node_type_to_str (node));

	/* Don't add the following tags if we are exporting to other applications */
	if (internal) {
		xmlNewProp (childNode, BAD_CAST"id", BAD_CAST node_get_id (node));

		switch (node->sortColumn) {
			case NODE_VIEW_SORT_BY_TITLE:
				xmlNewProp (childNode, BAD_CAST"sortColumn", BAD_CAST"title");
				break;
			case NODE_VIEW_SORT_BY_TIME:
				xmlNewProp (childNode, BAD_CAST"sortColumn", BAD_CAST"time");
				break;
			case NODE_VIEW_SORT_BY_PARENT:
				xmlNewProp (childNode, BAD_CAST"sortColumn", BAD_CAST"parent");
				break;
			case NODE_VIEW_SORT_BY_STATE:
				xmlNewProp (childNode, BAD_CAST"sortColumn", BAD_CAST"state");
				break;
			default:
				g_assert_not_reached();
				break;
		}

		if (FALSE == node->sortReversed)
			xmlNewProp (childNode, BAD_CAST"sortReversed", BAD_CAST"false");

		if (node->loadItemLink)
			xmlNewProp (childNode, BAD_CAST"loadItemLink", BAD_CAST"true");
	}

	/* 2. add node type specific stuff */
	NODE_TYPE (node)->export (node, childNode, internal);

	/* 3. add children */
	if (internal) {
		if (feed_list_view_is_expanded (node->id))
			xmlNewProp (childNode, BAD_CAST"expanded", BAD_CAST"true");
		else
			xmlNewProp (childNode, BAD_CAST"collapsed", BAD_CAST"true");
	}

	if (IS_FOLDER (node))
		export_node_children (node, childNode, internal);
}

static void
export_node_children (nodePtr node, xmlNodePtr cur, gboolean trusted)
{
	struct exportData	params;

	params.cur = cur;
	params.trusted = trusted;
	node_foreach_child_data (node, export_append_node_tag, &params);
}

gboolean
export_OPML_feedlist (const gchar *filename, nodePtr node, gboolean trusted)
{
	xmlDocPtr 	doc;
	xmlNodePtr 	cur, opmlNode;
	gboolean	error = FALSE;
	gchar		*backupFilename;
	int		old_umask = 0;

	debug_enter ("export_OPML_feedlist");

	backupFilename = g_strdup_printf ("%s~", filename);

	doc = xmlNewDoc (BAD_CAST"1.0");
	if (doc) {
		opmlNode = xmlNewDocNode (doc, NULL, BAD_CAST"opml", NULL);
		if (opmlNode) {
			xmlNewProp (opmlNode, BAD_CAST"version", BAD_CAST"1.0");

			/* create head */
			cur = xmlNewChild (opmlNode, NULL, BAD_CAST"head", NULL);
			if (cur)
				xmlNewTextChild (cur, NULL, BAD_CAST"title", BAD_CAST"Liferea Feed List Export");

			/* create body with feed list */
			cur = xmlNewChild (opmlNode, NULL, BAD_CAST"body", NULL);
			if (cur)
				export_node_children (node, cur, trusted);

			xmlDocSetRootElement (doc, opmlNode);
		} else {
			g_warning ("could not create XML feed node for feed cache document!");
			error = TRUE;
		}

		if (!trusted)
			old_umask = umask (022);	/* give read permissions for other, per-default we wouldn't give it... */

		xmlSetDocCompressMode (doc, 0);

		if (-1 == xmlSaveFormatFileEnc (backupFilename, doc, "utf-8", TRUE)) {
			g_warning ("Could not export to OPML file!");
			error = TRUE;
		}

		if (!trusted)
			umask (old_umask);

		xmlFreeDoc (doc);

		if (!error) {
			if (g_rename (backupFilename, filename) < 0) {
				g_warning (_("Error renaming %s to %s: %s\n"), backupFilename, filename, g_strerror (errno));
				error = TRUE;
			}
		}
	} else {
		g_warning ("Could not create XML document!");
		error = TRUE;
	}

	g_free (backupFilename);

	debug_exit ("export_OPML_feedlist");
	return !error;
}

static void
import_parse_outline (xmlNodePtr cur, nodePtr parentNode, gboolean trusted)
{
	gchar		*title, *typeStr, *tmp, *sortStr;
	xmlNodePtr	child;
	nodePtr		node;
	nodeTypePtr	type = NULL;
	gboolean	needsUpdate = FALSE;

	debug_enter("import_parse_outline");

	/* 1. determine node type */
	typeStr = (gchar *)xmlGetProp (cur, BAD_CAST"type");
	if (typeStr) {
		type = node_str_to_type (typeStr);
		xmlFree (typeStr);
	}

	/* if we didn't find a type attribute we use heuristics */
	if (!type) {
		/* check for a source URL */
		tmp = (gchar *)xmlGetProp (cur, BAD_CAST"xmlUrl");
		if (!tmp)
			tmp = (gchar *)xmlGetProp (cur, BAD_CAST"xmlurl");	/* AmphetaDesk */
		if (!tmp)
			tmp = (gchar *)xmlGetProp (cur, BAD_CAST"xmlURL");	/* LiveJournal */

		if (tmp) {
			debug0 (DEBUG_CACHE, "-> URL found assuming type feed");
			type = feed_get_node_type();
			xmlFree (tmp);
		} else {
			/* if the outline has no type and URL it just has to be a folder */
			type = folder_get_node_type();
			debug0 (DEBUG_CACHE, "-> must be a folder");
		}
	}

	g_assert (NULL != type);

	/* Check if adding this type is allowed */
	// FIXME: Prevent news bins outside root source
	// FIXME: Prevent search folders outside root source

	/* 2. do general node parsing */
	node = node_new (type);
	node_set_parent (node, parentNode, -1);

	/* The id should only be used from feedlist.opml. Otherwise,
	   it could cause corruption if the same id was imported
	   multiple times. */
	if (trusted) {
		gchar *id = NULL;
		id = (gchar *)xmlGetProp (cur, BAD_CAST"id");

		/* If, for some reason, the OPML has been corrupted
		   and there are two copies asking for a certain ID
		   then give the second one a new ID. */
		if (node_is_used_id (id)) {
			xmlFree (id);
			id = NULL;
		}

		if (id) {
			node_set_id (node, id);
			xmlFree (id);
		} else {
			needsUpdate = TRUE;
		}
	} else {
		needsUpdate = TRUE;
	}

	/* title */
	title = (gchar *)xmlGetProp (cur, BAD_CAST"title");
	if (!title || !xmlStrcmp ((xmlChar *)title, BAD_CAST"")) {
		if (title)
			xmlFree (title);
		title = (gchar *)xmlGetProp (cur, BAD_CAST"text");
	}

	if (title) {
		node_set_title (node, title);
		xmlFree (title);
	}

	/* sorting order */
	sortStr = (gchar *)xmlGetProp (cur, BAD_CAST"sortColumn");
	if (sortStr) {
		if (!xmlStrcmp ((xmlChar *)sortStr, BAD_CAST"title"))
			node->sortColumn = NODE_VIEW_SORT_BY_TITLE;
		else if (!xmlStrcmp ((xmlChar *)sortStr, BAD_CAST"parent"))
			node->sortColumn = NODE_VIEW_SORT_BY_PARENT;
		else if (!xmlStrcmp ((xmlChar *)sortStr, BAD_CAST"state"))
			node->sortColumn = NODE_VIEW_SORT_BY_STATE;
		else
			node->sortColumn = NODE_VIEW_SORT_BY_TIME;
		xmlFree (sortStr);
	}
	sortStr = (gchar *)xmlGetProp (cur, BAD_CAST"sortReversed");
	if (sortStr) {
		if(!xmlStrcmp ((xmlChar *)sortStr, BAD_CAST"false"))
			node->sortReversed = FALSE;
		xmlFree (sortStr);
	}

	/* auto item link loading flag */
	tmp = (gchar *)xmlGetProp (cur, BAD_CAST"loadItemLink");
	if (tmp) {
		if (!xmlStrcmp ((xmlChar *)tmp, BAD_CAST"true"))
		node->loadItemLink = TRUE;
		xmlFree (tmp);
	}

	/* expansion state */
	if (xmlHasProp (cur, BAD_CAST"expanded"))
		node->expanded = TRUE;
	else if (xmlHasProp (cur, BAD_CAST"collapsed"))
		node->expanded = FALSE;
	else
		node->expanded = TRUE;

	/* 3. Try to load the favicon (needs to be done before adding to the feed list) */
	node_load_icon (node);

	/* 4. add to GUI parent */
	feedlist_node_imported (node);

	/* 5. import child nodes */
	if (IS_FOLDER (node)) {
		child = cur->xmlChildrenNode;
		while (child) {
			if (!xmlStrcmp (child->name, BAD_CAST"outline"))
				import_parse_outline (child, node, trusted);
			child = child->next;
		}
	}

	/* 6. do node type specific parsing */
	NODE_TYPE (node)->import (node, parentNode, cur, trusted);

	if (node->subscription)
		liferea_auth_info_query (node->id);

	/* 7. update immediately if necessary */
	// FIXME: this should not be done here!!!
	if (node->subscription && needsUpdate) {
		debug1 (DEBUG_CACHE, "seems to be an import, setting new id: %s and doing first download...", node_get_id(node));
		subscription_update (node->subscription, 0);
	}

	/* 8. Always update the node info in the DB to ensure a proper
	   node entry and parent node information. Search folders would
	   silentely fail to work without node entry. */
	db_node_update (node);

	debug_exit ("import_parse_outline");
}

static void
import_parse_body (xmlNodePtr n, nodePtr parentNode, gboolean trusted)
{
	xmlNodePtr cur;

	cur = n->xmlChildrenNode;
	while (cur) {
		if (!xmlStrcmp (cur->name, BAD_CAST"outline"))
			import_parse_outline (cur, parentNode, trusted);
		cur = cur->next;
	}
}

static void
import_parse_OPML (xmlNodePtr n, nodePtr parentNode, gboolean trusted)
{
	xmlNodePtr cur;

	cur = n->xmlChildrenNode;
	while (cur) {
		/* we ignore the head */
		if (!xmlStrcmp (cur->name, BAD_CAST"body")) {
			import_parse_body (cur, parentNode, trusted);
		}
		cur = cur->next;
	}
}

gboolean
import_OPML_feedlist (const gchar *filename, nodePtr parentNode, gboolean showErrors, gboolean trusted)
{
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	gboolean	error = FALSE;

	debug1 (DEBUG_CACHE, "Importing OPML file: %s", filename);

	/* read the feed list */
	doc = xmlParseFile (filename);
	if (!doc) {
		if (showErrors)
			ui_show_error_box (_("XML error while reading OPML file! Could not import \"%s\"!"), filename);
		else
			g_warning (_("XML error while reading OPML file! Could not import \"%s\"!"), filename);
		error = TRUE;
	} else {
		cur = xmlDocGetRootElement (doc);
		if (!cur) {
			if (showErrors)
				ui_show_error_box (_("Empty document! OPML document \"%s\" should not be empty when importing."), filename);
			else
				g_warning (_("Empty document! OPML document \"%s\" should not be empty when importing."), filename);
			error = TRUE;
		} else {
			if (!trusted) {
				/* set title only when importing as folder and not as OPML source */
				xmlNodePtr title = xpath_find (cur, "/opml/head/title");
				if (title) {
					xmlChar *titleStr = xmlNodeListGetString (title->doc, title->xmlChildrenNode, 1);
					if (titleStr) {
						node_set_title (parentNode, (gchar *)titleStr);
						xmlFree (titleStr);
					}
				}
			}

			while (cur) {
				if (!xmlIsBlankNode (cur)) {
					if (!xmlStrcmp (cur->name, BAD_CAST"opml")) {
						import_parse_OPML (cur, parentNode, trusted);
					} else {
						if (showErrors)
							ui_show_error_box (_("\"%s\" is not a valid OPML document! Liferea cannot import this file!"), filename);
						else
							g_warning (_("\"%s\" is not a valid OPML document! Liferea cannot import this file!"), filename);
					}
				}
				cur = cur->next;
			}
		}
		xmlFreeDoc (doc);
	}

	return !error;
}

/* UI stuff */

static void
on_import_activate_cb (const gchar *filename, gpointer user_data)
{
	if (filename) {
		nodePtr node = node_new (folder_get_node_type ());
		node_set_title (node, _("Imported feed list"));
		feedlist_node_added (node);

		if (!import_OPML_feedlist (filename, node, TRUE /* show errors */, FALSE /* not trusted */)) {
			feedlist_remove_node (node);
		}
	}
}

void
import_OPML_file (void)
{
	ui_choose_file(_("Import Feed List"), _("Import"), FALSE, on_import_activate_cb, NULL, NULL, "*.opml|*.xml", _("OPML Files"), NULL);
}

static void
on_export_activate_cb (const gchar *filename, gpointer user_data)
{
	if (filename) {
		if (!export_OPML_feedlist (filename, feedlist_get_root (), FALSE))
			ui_show_error_box (_("Error while exporting feed list!"));
		else
			ui_show_info_box (_("Feed List exported!"));
	}
}

void
export_OPML_file (void)
{
	ui_choose_file (_("Export Feed List"), _("Export"), TRUE, on_export_activate_cb,  NULL, "feedlist.opml", "*.opml", _("OPML Files"), NULL);
}
