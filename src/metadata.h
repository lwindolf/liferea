/**
 * @file metadata.h Metadata storage API
 *
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _METADATA_H
#define _METADATA_H

#include <glib.h>
#include <libxml/tree.h>
#include "feed.h"

/* -------------------------------------------------------- */
/* interface definitions for namespace parsing handler      */
/* -------------------------------------------------------- */

/** definition of various namespace tag handler */
typedef void	(*parseChannelTagFunc)	(feedPtr fp, xmlNodePtr cur);
typedef void	(*parseItemTagFunc)	(itemPtr ip, xmlNodePtr cur);

/** struct used to register namespace handler */
typedef struct NsHandler {
	gchar			*prefix;		/**< namespace prefix */

	parseItemTagFunc	parseItemTag;		/**< item tag parsing method */
	parseChannelTagFunc	parseChannelTag;	/**< channel tag parsing method */
} NsHandler;

/* -------------------------------------------------------- */
/* interface definitions for namespace rendering handler    */
/* -------------------------------------------------------- */

struct displayset;

typedef void (*renderHTMLFunc)(gpointer data, struct displayset *displayset, gpointer user_data);

/** Initialize the metadata subsystem */
void metadata_init();

/** Register a new type of metadata */
void metadata_register(const gchar *strid, renderHTMLFunc renderfunc, gpointer user_data);

/** 
 * Appends a value to the value list of a specific metadata type 
 * Don't mix this function with metadata_list_set() !
 */
GSList * metadata_list_append(GSList *metadata, const gchar *strid, const gchar *data);

/** 
 * Sets (and overwrites if necessary) the value of a specific metadata type.
 * Don't mix this function with metadata_list_append() !
 */
void metadata_list_set(GSList **metadata, const gchar *strid, const gchar *data);

void metadata_list_render(GSList *metadata, struct displayset *displayset);

void metadata_list_free(GSList *metadata);

void metadata_add_xml_nodes(GSList *metadata, xmlNodePtr parentNode);

GSList * metadata_parse_xml_nodes(xmlDocPtr, xmlNodePtr cur);

#endif
