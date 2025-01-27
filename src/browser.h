/**
 * @file browser.h  Launching different external browsers
 *
 * Copyright (C) 2008-2015 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _BROWSER_H
#define _BROWSER_H

#include <glib.h>

#include "item.h"

/**
 * Function to execute the commands needed to open up a URL with the
 * browser specified in the preferences.
 *
 * @param the URI to load
 *
 * @returns TRUE if the URI was opened, or FALSE if there was an error
 */

gboolean browser_launch_URL_external (const gchar *uri);

/**
 * browser_launch_URL:
 * @url:	        the link to load
 * @internal:	TRUE if internal browsing is to be enforced
 *
 * Launch the given URL in the currently active HTML view.
 *
 */
void browser_launch_URL (const gchar *url, gboolean internal);


typedef enum {
	BROWSER_LAUNCH_DEFAULT,
	BROWSER_LAUNCH_INTERNAL,
	BROWSER_LAUNCH_TAB,
	BROWSER_LAUNCH_EXTERNAL
} open_link_target_type;

/**
 * browser_launch_item:
 * @item:		the item to launch
 * @open_link_target:	the target to open the link in
 *
 * Launches the item in the given target.
 */
void browser_launch_item (itemPtr item, open_link_target_type open_link_target);

#endif
