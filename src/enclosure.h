/**
 * @file enclosure.h enclosure/podcast support
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

#ifndef _ENCLOSURE_H
#define _ENCLOSURE_H

/** structure describing a supported download tool */
typedef struct enclosureDownloadTool {
	const char	*format;	/**< format string to construct download command */
	gboolean	niceFilename;	/**< TRUE if format has second %s for output file name */
} *enclosureDownloadToolPtr; 

/** structure describing the preferences for a MIME type or file extension */
typedef struct encType {
	gchar		*mime;		/**< either mime or extension is set */
	gchar		*extension;
	gchar		*cmd;		/**< the command to launch the enclosure type */
	gboolean	permanent;	/**< if TRUE definition is deleted after opening and 
					     not added to the permanent list of type configs */
	gboolean	remote;		/**< if TRUE enclosure is to be opened without downloading (pass URL only) */
} *encTypePtr;

/** structure describing an enclosure and its states */
typedef struct enclosure {
	gchar		*url;		/**< enclosure download URI (absolute path) */
	gchar		*mime;		/**< enclosure MIME type (optional, can be NULL) */
	gsize		size;		/**< enclosure size (optional, can be 0) */
	gboolean	downloaded;	/**< flag indicating we have downloaded the enclosure */
} *enclosurePtr;

/**
 * Parses enclosure description.
 *
 * @param str		the enclosure description
 *
 * @returns new enclosure structure (to be free'd using enclosure_free)
 */
enclosurePtr enclosure_from_string (const gchar *str);

/**
 * Serialize enclosure infos to string.
 *
 * @param url		the enclosure URL
 * @param mime		the MIME type (optional, can be NULL)
 * @param size  	the enclosure size (optional, can be 0)
 * @param downloaded	downloading state (TRUE=downloaded)
 *
 * @returns new string (to be free'd using g_free)
 */
gchar * enclosure_values_to_string (const gchar *url, const gchar *mime, gsize size, gboolean downloaded);

/**
 * Serialize enclosre to string.
 *
 * @param encloszre	the enclosure
 *
 * @returns new string (to be free'd using g_free)
 */
gchar * enclosure_to_string (const enclosurePtr enclosure);

/**
 * Free all memory associated with the enclosure.
 *
 * @oparam enclosure	the enclosure
 */
void enclosure_free (enclosurePtr enclosure);

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
