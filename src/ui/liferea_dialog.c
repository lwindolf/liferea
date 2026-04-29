/**
 * @file liferea_dialog.c UI dialog handling
 *
 * Copyright (C) 2007-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include <gtk/gtk.h>

#include "ui/liferea_dialog.h"

#include "ui/liferea_shell.h"

typedef struct {
	GtkBuilder *xml;

	GtkWidget	*dialog;
} LifereaDialogPrivate;

struct _LifereaDialog {
	GObject parent_instance;
};

G_DEFINE_TYPE_WITH_PRIVATE (LifereaDialog, liferea_dialog, G_TYPE_OBJECT)

static void
liferea_dialog_finalize (GObject *object)
{
	LifereaDialog *ld = LIFEREA_DIALOG (object);
	LifereaDialogPrivate *priv = liferea_dialog_get_instance_private (ld);

	g_object_unref (priv->xml);

	G_OBJECT_CLASS (liferea_dialog_parent_class)->finalize (object);
}

static void
liferea_dialog_class_init (LifereaDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = liferea_dialog_finalize;
}

static void
liferea_dialog_init (LifereaDialog *ld)
{
}

GtkWidget *
liferea_dialog_lookup (GtkWidget *widget, const gchar *name)
{
	LifereaDialog	*ld;

	if (!widget)
		return NULL;

	ld = LIFEREA_DIALOG (g_object_get_data (G_OBJECT (widget), "LifereaDialog"));

	if (!LIFEREA_IS_DIALOG (ld)) {
		g_warning ("Fatal: liferea_dialog_lookup() called with something that is not a Liferea dialog!");
		return NULL;
	}

	LifereaDialogPrivate *priv = liferea_dialog_get_instance_private (ld);
	if (priv->xml)
		return GTK_WIDGET (gtk_builder_get_object (priv->xml, name));
	return NULL;
}


GtkWidget *
liferea_dialog_new (const gchar *name)
{
	LifereaDialog	*ld;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *path = g_strdup_printf ("/org/gnome/liferea/ui/%s.ui", name);

	ld = LIFEREA_DIALOG (g_object_new (LIFEREA_DIALOG_TYPE, NULL));
	LifereaDialogPrivate *priv = liferea_dialog_get_instance_private (ld);
	priv->xml = gtk_builder_new ();

	if (!gtk_builder_add_from_resource (priv->xml, path, &error)) {
		g_warning ("Could not load dialog UI file '%s': %s", path, error->message);
		g_object_unref (ld);
		return NULL;
	}

	priv->dialog = GTK_WIDGET (gtk_builder_get_object (priv->xml, name));
	if (!priv->dialog) {
		g_warning ("Dialog '%s' not found in UI file", name);
		g_object_unref (ld);
		return NULL;
	}

	gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), GTK_WINDOW (liferea_shell_get_window ()));
	g_return_val_if_fail (priv->dialog != NULL, NULL);

	g_object_set_data (G_OBJECT (priv->dialog), "LifereaDialog", ld);

	g_signal_connect_swapped (priv->dialog, "destroy", 
							  G_CALLBACK (g_object_unref), ld);

	return priv->dialog;
}

const gchar *
liferea_dialog_entry_get (GtkWidget *dialog, const gchar *name)
{
	return gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (liferea_dialog_lookup (dialog, name))));
}

void
liferea_dialog_entry_set (GtkWidget *dialog, const gchar *name, const gchar *text)
{
	gtk_entry_buffer_set_text (gtk_entry_get_buffer (GTK_ENTRY (liferea_dialog_lookup (dialog, name))), text, -1);
}
