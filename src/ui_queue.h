/*
 * GUI callback managment
 *
 * Most of this code was derived from 
 *
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
 *
 * and modified to be suitable for Liferea
 *
 * Copyright (C) 2004  Lars Lindner <lars.lindner@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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

#ifndef _UI_QUEUE_H
#define _UI_QUEUE_H
 
#include <glib.h>
#include <gtk/gtk.h>

/* GUI callback queue methods */
void	ui_queue_init(void);
void	ui_queue_shutdown(void);
void	ui_queue_remove(guint queue_id);
guint	ui_queue_add(GSourceFunc run_func, gpointer user_data);

/* GUI locking */
guint	ui_timeout_add(guint32 interval, GSourceFunc function, gpointer data);

#ifdef __GNUC__
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define PRETTY_FUNCTION ""
#endif

#define ui_lock() ui_lock_from(__FILE__, PRETTY_FUNCTION, __LINE__)
void ui_lock_from (const char * file, const char * func, int line);

#define ui_unlock() ui_unlock_from(__FILE__, PRETTY_FUNCTION, __LINE__)
void ui_unlock_from (const char * file, const char * func, int line);

#endif
