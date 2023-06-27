/**
 * Debugging output support.
 *
 * Copyright (C) 2023  Lars Windolf <lars.windolf@gmx.de>
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

#ifndef __DEBUG_H__
#define __DEBUG_H__

typedef enum
{
	DEBUG_CACHE		= (1<<0),
	DEBUG_CONF		= (1<<1),
	DEBUG_UPDATE		= (1<<2),
	DEBUG_PARSING		= (1<<3),
	DEBUG_GUI		= (1<<4),
	DEBUG_HTML		= (1<<6),
	DEBUG_NET		= (1<<7),
	DEBUG_DB		= (1<<8),
	DEBUG_VFOLDER		= (1<<10)
}
DebugFlags;

/**
 * Enable debugging for one or more of the given debugging flags.
 *
 * @param flags		debugging flags (see above)
 */
void debug_set_flags (gulong flags);

/**
 * Returns current debug flags
 */
gulong debug_get_flags (void);

/**
 * Debug print function
 * 
 * @flags: the topic mask top print at
 */
void debug (gulong flags, const char* fmt, ...);

#endif
