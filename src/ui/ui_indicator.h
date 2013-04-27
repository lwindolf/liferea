/*
 * @file ui_indicator.h  libindicate support
 *
 * Copyright (C) 2010-2011 Maia Kozheva <sikon@ubuntu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#ifndef _UI_INDICATOR_H
#define _UI_INDICATOR_H

#include <glib.h>

/**
 * Setup indicator widget.
 */
void ui_indicator_init (void);

/**
 * Destroy indicator widget.
 */
void ui_indicator_destroy (void);

/**
 * To be called whenever indicator widget should be updated.
 */
void ui_indicator_update (void);

/**
 * Query whether indicator widget is active.
 * 
 * @returns TRUE if indicator widget is active
 */
gboolean ui_indicator_is_visible (void);

#endif  /* _UI_INDICATOR_H */
