/**
 * @file metadata.h Metadata storage API
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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
#include "htmlview.h"
struct displayset;

/**
 * @param prevData The previous data contained by the attribute, or
 * NULL if this attribute has not been set yet. This could either
 * replace the current data, or append it to a linked list.

 * @param str The string representation of the attribute
 *
 * @returns the new data to use as the in-memory representation of the
 * piece of metadata.
 */
typedef gpointer (*parserFunc)(gpointer prevData, const gchar *str, gpointer user_data);

typedef void (*renderHTMLFunc)(gpointer data, struct displayset *displayset, gpointer user_data);

typedef void (*freeDataFunc)(gpointer data, gpointer user_data);

/** Initialize the metadata subsystem */
void metadata_init();

/** Register a new type of metadata */
void metadata_register(const gchar *strid, parserFunc pfunc, renderHTMLFunc renderfunc, freeDataFunc freefunc, gpointer user_data);

gpointer metadata_list_insert_strid(gpointer metadataList, const gchar *strid, const gchar *data);

void metadata_list_render(gpointer metadataList, struct displayset *displayset);
#endif
