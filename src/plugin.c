/**
 * @file plugin.c Liferea plugin implementation
 * 
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
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

#include "plugin.h"

/* plugin managment */

/** list of all loaded plugins */
GSList *plugins = NULL;

void plugin_mgmt_init(void) {
}

void plugin_mgmt_deinit(void) {
	// FIXME
}

void plugin_mgmt_load(void) {
	// FIXME: scan for loadable modules
}

/* common plugin methods */

void plugin_enable(guint id) {

	// FIXME: set gconf key to true
}

void plugin_disable(guint id) {

	// FIXME: set gconf key to false
}

gboolean plugin_get_active(guint id) {

	// FIXME: return enabled state from gconf
	return TRUE;
}
