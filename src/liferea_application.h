/*
 * @file liferea_application.h  LifereaApplication type
 *
 * Copyright (C) 2016 Leiaz <leiaz@free.fr>
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

#ifndef _LIFEREA_APPLICATION_H
#define _LIFEREA_APPLICATION_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define LIFEREA_APPLICATION_TYPE (liferea_application_get_type ())
G_DECLARE_FINAL_TYPE (LifereaApplication, liferea_application, LIFEREA, APPLICATION, GtkApplication)

/**
 * liferea_application_new: (skip)
 * @argc: number of arguments
 * @argv: arguments
 *
 * Start a new GApplication representing Liferea
 *
 * Returns: exit code
 */
gint liferea_application_new (int argc, char *argv[]);

/**
 * liferea_application_shutdown:
 *
 * Shutdown GApplication
 */
void liferea_application_shutdown (void);

G_END_DECLS

#endif
