/**
 * @file ui_dialog.c UI dialog handling
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

#include "ui/ui_dialog.h"
#include "ui/liferea_shell.h"

static void liferea_dialog_class_init	(LifereaDialogClass *klass);
static void liferea_dialog_init		(LifereaDialog *ld);

#define LIFEREA_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), LIFEREA_DIALOG_TYPE, LifereaDialogPrivate))

struct LifereaDialogPrivate {
	GladeXML	*xml;
	
	GtkWidget	*dialog;
};

static GObjectClass *parent_class = NULL;

GType
liferea_dialog_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (LifereaDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) liferea_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (LifereaDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) liferea_dialog_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "LifereaDialog",
					       &our_info, 0);
	}

	return type;
}

static void
liferea_dialog_finalize (GObject *object)
{
	LifereaDialog *ls = LIFEREA_DIALOG (object);
	
	g_object_unref (ls->priv->xml);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
liferea_dialog_destroy_cb (GtkWidget *widget, LifereaDialog *ld)
{
	g_object_unref (ld);
}

static void
liferea_dialog_class_init (LifereaDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = liferea_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(LifereaDialogPrivate));
}

static void
liferea_dialog_init (LifereaDialog *ld)
{
	ld->priv = LIFEREA_DIALOG_GET_PRIVATE (ld);
}

GtkWidget *
liferea_dialog_lookup (GtkWidget *widget, const gchar *name)
{
	LifereaDialog	*ld;
	
	if (!widget)
		return NULL;
		
	ld = LIFEREA_DIALOG (g_object_get_data (G_OBJECT (widget), "LifereaDialog"));
		
	if (!IS_LIFEREA_DIALOG (ld)) {
		g_warning ("Fatal: liferea_dialog_lookup() called with something that is not a Liferea dialog!");
		return NULL;
	}
	
	if (ld->priv->xml)
		return glade_xml_get_widget (ld->priv->xml, name);
		
	return NULL;
}


GtkWidget *
liferea_dialog_new (const gchar *filename, const gchar *name) 
{
	LifereaDialog	*ld;
	gchar 		*path;
	
	ld = LIFEREA_DIALOG (g_object_new (LIFEREA_DIALOG_TYPE, NULL));

	path = g_strdup_printf ("%s%s", PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S, filename?filename:"liferea.glade");
	ld->priv->xml = glade_xml_new (path, name, GETTEXT_PACKAGE);
	g_free (path);
	
	g_return_val_if_fail (ld->priv->xml != NULL, NULL);

	ld->priv->dialog = glade_xml_get_widget (ld->priv->xml, name);
	glade_xml_signal_autoconnect (ld->priv->xml);
	g_return_val_if_fail (ld->priv->dialog != NULL, NULL);
	
	g_object_set_data (G_OBJECT (ld->priv->dialog), "LifereaDialog", ld);
	
	gtk_window_set_transient_for (GTK_WINDOW (ld->priv->dialog), GTK_WINDOW (liferea_shell_lookup ("mainwindow")));

	g_signal_connect_object (ld->priv->dialog, "destroy", G_CALLBACK (liferea_dialog_destroy_cb), ld, 0);
	
	return ld->priv->dialog;
}
