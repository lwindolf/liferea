/**
 * @file ui_dialog.c UI dialog handling
 *
 * Copyright (C) 2007-2016 Lars Windolf <lars.windolf@gmx.de>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "ui/liferea_dialog.h"

#include "ui/liferea_shell.h"

#define LIFEREA_DIALOG_GET_PRIVATE liferea_dialog_get_instance_private

struct LifereaDialogPrivate {
	GtkBuilder *xml;

	GtkWidget	*dialog;
};

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE_WITH_CODE (LifereaDialog, liferea_dialog, G_TYPE_OBJECT, G_ADD_PRIVATE (LifereaDialog));

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
		return GTK_WIDGET (gtk_builder_get_object (ld->priv->xml, name));

	return NULL;
}


GtkWidget *
liferea_dialog_new (const gchar *name)
{
	LifereaDialog	*ld;
	gchar 		*path;
	GError          *error = NULL;

	ld = LIFEREA_DIALOG (g_object_new (LIFEREA_DIALOG_TYPE, NULL));
	path = g_strdup_printf ("%s%s.ui", PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S, name);
	ld->priv->xml = gtk_builder_new_from_file (path);
	g_free (path);

	g_return_val_if_fail (ld->priv->xml != NULL, NULL);

	ld->priv->dialog = GTK_WIDGET (gtk_builder_get_object (ld->priv->xml, name));
	gtk_window_set_transient_for (GTK_WINDOW (ld->priv->dialog), GTK_WINDOW (liferea_shell_get_window()));
	gtk_builder_connect_signals (ld->priv->xml, NULL);
	g_return_val_if_fail (ld->priv->dialog != NULL, NULL);

	g_object_set_data (G_OBJECT (ld->priv->dialog), "LifereaDialog", ld);

	g_signal_connect_object (ld->priv->dialog, "destroy", G_CALLBACK (liferea_dialog_destroy_cb), ld, 0);

	return ld->priv->dialog;
}
