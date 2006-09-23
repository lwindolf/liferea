/**
 * @file export.h OPML feedlist import&export
 * 
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _EXPORT_H
#define _EXPORT_H

#include <gtk/gtk.h>
#include "node.h"
#include "fl_sources/node_source.h"

/**
 * Exports a given feed list tree. Can be used to export
 * the static feed list but also to export subtrees.
 *
 * @param filename	filename of export file
 * @param node		root node of the tree to export
 * @param internal	FALSE if export to other programs
 *			is requested, will suppress export
 *			of passwords and Liferea specifics
 *
 * @returns TRUE on success
 */
gboolean export_OPML_feedlist(const gchar *filename, nodePtr node, gboolean internal);

/**
 * Exports the given node children by calling their node type
 * specific export methods to add outline tags to the given
 * XML node.
 *
 * @param parentNode	the parent node
 * @param cur		the outline XML node
 * @param nodeSource	the node source
 */
void export_node_children(nodePtr node, xmlNodePtr cur, gboolean trusted);

/**
 * Reads an OPML file and inserts it into the feedlist.
 *
 * @param filename	path to file that will be read for importing
 * @param parentNode	node of the parent folder
 * @param nodeSource	the feed list source instance to handle the nodes
 * @param showErrors	set to TRUE if errors should generate a error dialog
 * @param trusted	set to TRUE if the feedlist is being imported from a trusted source
 *
 * @returns TRUE on success
 */
gboolean import_OPML_feedlist(const gchar *filename, nodePtr parentNode, nodeSourcePtr nodeSource, gboolean showErrors, gboolean trusted);

/**
 * Parses the given outline XML node and depending on the
 * node type attribute calls the node specific import method.
 *
 * @param cur		the outline XML node
 * @param parentNode	the parent node
 * @param nodeSource	the node source
 * @param trusted	set to TRUE if the feedlist is being imported from a trusted source
 */
void import_parse_outline(xmlNodePtr cur, nodePtr parentNode, nodeSourcePtr nodeSource, gboolean trusted);

/* GUI dialog callbacks */

void on_import_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_export_activate(GtkMenuItem *menuitem, gpointer user_data);

#endif
