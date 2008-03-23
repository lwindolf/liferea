/**
 * @file ui_common.c  UI helper functions
 *
 * Copyright (C) 2008 Lars Lindner <lars.lindner@gmail.com>
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

#include "ui_common.h"

#include "common.h"

void
ui_common_setup_combo_menu (GtkWidget *widget,
                     gchar **options,
                     GCallback callback,
                     gint defaultValue)
{
	GtkListStore	*listStore;
	GtkTreeIter	treeiter;
	guint		i;
	
	listStore = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
	g_assert (NULL != widget);
	for (i = 0; options[i] != NULL; i++) {
		gtk_list_store_append (listStore, &treeiter);
		gtk_list_store_set (listStore, &treeiter, 0, _(options[i]), 1, i, -1);
	}
	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (listStore));
	if (-1 <= defaultValue)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), defaultValue);
	
	if (callback)	
		g_signal_connect (G_OBJECT (widget), "changed", callback, widget);
}
