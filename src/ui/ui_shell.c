/**
 * @file ui_shell.c UI handling
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "ui/ui_shell.h"

static void liferea_shell_class_init	(LifereaShellClass *klass);
static void liferea_shell_init		(LifereaShell *ls);

#define LIFEREA_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), LIFEREA_SHELL_TYPE, LifereaShellPrivate))

struct LifereaShellPrivate {
	GladeXML	*xml;

	GtkWidget	*mainwindow;
};

static GObjectClass *parent_class = NULL;

LifereaShell *liferea_shell = NULL;

GType
liferea_shell_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (LifereaShellClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) liferea_shell_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (LifereaShell),
			0, /* n_preallocs */
			(GInstanceInitFunc) liferea_shell_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "LifereaShell",
					       &our_info, 0);
	}

	return type;
}

static void
liferea_shell_finalize (GObject *object)
{
	LifereaShell *ls = LIFEREA_SHELL (object);
	
	g_object_unref (ls->priv->xml);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
liferea_shell_class_init (LifereaShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = liferea_shell_finalize;

	g_type_class_add_private (object_class, sizeof(LifereaShellPrivate));
}

GtkWidget *
liferea_shell_lookup (const gchar *name)
{
	g_return_val_if_fail (liferea_shell != NULL, NULL);
	g_return_val_if_fail (liferea_shell->priv != NULL, NULL);

	return glade_xml_get_widget (liferea_shell->priv->xml, name);
}

static void
liferea_shell_init (LifereaShell *ls)
{
	/* globally accessible singleton */
	g_assert (NULL == liferea_shell);
	liferea_shell = ls;
	
	ls->priv = LIFEREA_SHELL_GET_PRIVATE (ls);
	ls->priv->xml = glade_xml_new (PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "liferea.glade", "mainwindow", GETTEXT_PACKAGE);
	glade_xml_signal_autoconnect (ls->priv->xml);
}

void
liferea_shell_create (void)
{
	g_object_new (LIFEREA_SHELL_TYPE, NULL);
}
