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

#define LIFEREA_TYPE_APPLICATION (liferea_application_get_type ())
#define LIFEREA_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_TYPE_APPLICATION, LifereaApplication))
#define IS_LIFEREA_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_APPLICATION_TYPE))
#define LIFEREA_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_TYPE_APPLICATION, LifereaApplicationClass))
#define IS_LIFEREA_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_TYPE_APPLICATION))
#define LIFEREA_APPLICATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), LIFEREA_TYPE_APPLICATION, LifereaApplicationClass))

typedef struct _LifereaApplication LifereaApplication;

typedef struct _LifereaApplicationClass LifereaApplicationClass;

GType liferea_application_get_type ();

/*
 * Start a new GApplication representing Liferea
 *
 * @param argc  number of arguments
 * @param argv  arguments
 *
 * Returns: exit code
 */
gint liferea_application_new (int argc, char *argv[]);

/*
 * Shutdown GApplication
 */
void liferea_application_shutdown (void);

#endif
