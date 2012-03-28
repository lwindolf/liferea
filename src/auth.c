/**
 * @file auth.c  authentication helpers
 *
 * Copyright (C) 2012 Lars Lindner <lars.lindner@gmail.com>
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

#include "auth.h"

#include "node.h"
#include "update.h"

void
liferea_auth_info_add (const gchar *id, const gchar *username, const gchar *password)
{
	// FIXME: Return if auth info already provided
	g_print ("Got auth info for %s: %s %s\n", id, username, password);
}
