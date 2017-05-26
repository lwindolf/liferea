/**
 * @file migrate.h migration between different cache versions
 * 
 * Copyright (C) 2007-2011 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _MIGRATE_H
#define _MIGRATE_H

#include "node.h"

typedef enum {
	MIGRATION_MODE_INVALID = 0,
	MIGRATION_FROM_14,
	MIGRATION_FROM_16,
	MIGRATION_FROM_18
} migrationMode;

/**
 * Performs a migration for the given migration mode.
 *
 * @param mode	migration mode
 * @param node	feed list root node
 */
void migration_execute (migrationMode mode, nodePtr node);

#endif
