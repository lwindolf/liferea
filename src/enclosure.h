/**
 * @file enclosure.h enclosure/podcast support
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

#ifndef _ENCLOSURE_H
#define _ENCLOSURE_H

typedef struct enclosureDownloadTool {
	const char	*format;	/**< format string to construct download command */
	gboolean	niceFilename;	/**< TRUE if format has second %s for output file name */
} *enclosureDownloadToolPtr; 

typedef struct encType {
	gchar		*mime;		/**< either mime or extension is set */
	gchar		*extension;
	gchar		*cmd;		/**< the command to launch the enclosure type */
	gboolean	permanent;	/**< if TRUE definition is deleted after opening and 
					     not added to the permanent list of type configs */
	gboolean	remote;		/**< if TRUE enclosure is to be opened without downloading (pass URL only) */
} *encTypePtr;

/**
 * Returns all configured enclosure types.
 *
 * @returns list of encType structures
 */
const GSList const * enclosure_mime_types_get (void);

/**
 * Adds a new MIME type handling definition.
 *
 * @param type	the new definition
 */
void enclosure_mime_type_add (encTypePtr type);

/** 
 * Removes an existing MIME type handling definition.
 * The definition will be free'd by this function.
 *
 * @param type	the definition to remove
 */
void enclosure_mime_type_remove (encTypePtr type);

/** 
 * Downloads a given enclosure URL into a file
 *  
 * @param type		NULL or pointer to type structure
 * @param url		valid HTTP URL
 * @param filename	valid filename
 */
void enclosure_save_as_file (encTypePtr type, const gchar *url, const gchar *filename);

#endif
