/**
 * @file ui_shell.h UI handling
 *
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifndef _UI_SHELL_H
#define _UI_SHELL_H
 
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define LIFEREA_SHELL_TYPE		(liferea_shell_get_type ())
#define LIFEREA_SHELL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_SHELL_TYPE, LifereaShell))
#define LIFEREA_SHELL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_SHELL_TYPE, LifereaShellClass))
#define IS_LIFEREA_SHELL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_SHELL_TYPE))
#define IS_LIFEREA_SHELL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_SHELL_TYPE))

typedef struct LifereaShell		LifereaShell;
typedef struct LifereaShellClass	LifereaShellClass;
typedef struct LifereaShellPrivate	LifereaShellPrivate;

extern LifereaShell *liferea_shell;

struct LifereaShell
{
	GObject		parent;
	
	/*< private >*/
	LifereaShellPrivate	*priv;
};

struct LifereaShellClass 
{
	GtkObjectClass parent_class;
};

GType liferea_shell_get_type	(void);

/**
 * Searches the glade XML UI tree for the given widget
 * name and returns the found widget.
 *
 * @param name	the widget name
 *
 * @returns the found widget or NULL
 */
GtkWidget * liferea_shell_lookup (const gchar *name);

/**
 * Initially setup shell which can be afterwards 
 * accessed by the global liferea_shell object.
 */
void liferea_shell_create (void);

// FIXME:
gboolean on_quit(GtkWidget *widget, GdkEvent *event, gpointer user_data);
gboolean quit(gpointer user_data);

G_END_DECLS
 
#endif
