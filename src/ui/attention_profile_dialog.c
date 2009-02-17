/**
 * @file ui_attention.c attention profile dialog
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

#include "ui/attention_profile_dialog.h"

#include <glib.h>

#include "common.h"
#include "ui/liferea_dialog.h"

static void attention_profile_dialog_class_init	(AttentionProfileDialogClass *klass);
static void attention_profile_dialog_init	(AttentionProfileDialog *apd);

#define ATTENTION_PROFILE_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), ATTENTION_PROFILE_DIALOG_TYPE, AttentionProfileDialogPrivate))

struct AttentionProfileDialogPrivate {
	AttentionProfile	*ap;

	GtkWidget	*dialog;
	GtkWidget	*treeview;
	GtkTreeStore	*treestore;
	GHashTable	*categoryToIter;

	guint		updateTimeout;
};

enum {
	APS_NAME_STR,
	APS_COUNT_STR,
	APS_COUNT,
	APS_LEN
};

static AttentionProfileDialog *singleton = NULL;

static GObjectClass *parent_class = NULL;

GType
attention_profile_dialog_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (AttentionProfileDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) attention_profile_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (AttentionProfileDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) attention_profile_dialog_init,
			NULL /* value_table */
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "AttentionProfileDialog",
					       &our_info, 0);
	}

	return type;
}

static void
attention_profile_dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
attention_profile_dialog_destroy_cb (GtkWidget *widget, AttentionProfileDialog *apd)
{
	g_source_remove (apd->priv->updateTimeout);
	g_hash_table_destroy (apd->priv->categoryToIter);
	g_object_unref (apd);
	singleton = NULL;
}

static void
attention_profile_dialog_class_init (AttentionProfileDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = attention_profile_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(AttentionProfileDialogPrivate));
}

static void
attention_profile_dialog_init (AttentionProfileDialog *apd)
{
	apd->priv = ATTENTION_PROFILE_DIALOG_GET_PRIVATE (apd);
	apd->priv->categoryToIter = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static gboolean
attention_profile_dialog_update (void *data)
{
	AttentionProfileDialog	*apd = ATTENTION_PROFILE_DIALOG (data);
	GSList			*iter;

	iter = attention_profile_get_categories (apd->priv->ap);
	while (iter) {
		categoryStatPtr	stat = (categoryStatPtr)iter->data;
		GtkTreeIter	*treeIter;
		gchar		*tmp;

		treeIter = g_hash_table_lookup (apd->priv->categoryToIter, stat->id);
		if (!treeIter) {
			treeIter = g_new0 (GtkTreeIter, 1);
			gtk_tree_store_append (apd->priv->treestore, treeIter, NULL);
			g_hash_table_insert (apd->priv->categoryToIter, g_strdup (stat->id), treeIter);
		}

		tmp = g_strdup_printf ("%lu", stat->count);
		gtk_tree_store_set (apd->priv->treestore, treeIter,
		                    APS_NAME_STR, stat->name,
				    APS_COUNT_STR, tmp,
				    APS_COUNT, stat->count,
				    -1);
		g_free (tmp);

		iter = g_slist_next (iter);
	}

	return TRUE;
}

AttentionProfileDialog *
attention_profile_dialog_open (AttentionProfile *ap)
{
	AttentionProfileDialog	*apd;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn	*column;

	if (singleton)
		return singleton;

	singleton = apd = ATTENTION_PROFILE_DIALOG (g_object_new (ATTENTION_PROFILE_DIALOG_TYPE, NULL));
	apd->priv->ap = ap;
	apd->priv->dialog = liferea_dialog_new (NULL, "attentiondialog");
	apd->priv->treeview = liferea_dialog_lookup (apd->priv->dialog, "attentiontreeview");
	apd->priv->treestore = gtk_tree_store_new (APS_LEN,
	                                           G_TYPE_STRING,	/* APS_NAME_STR */
						   G_TYPE_STRING,	/* APS_COUNT_STR */
						   G_TYPE_INT		/* APS_COUNT */
	                                           );
	gtk_tree_view_set_model (GTK_TREE_VIEW (apd->priv->treeview), GTK_TREE_MODEL(apd->priv->treestore));

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Category"), renderer,
	                                                   "text", APS_NAME_STR,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (apd->priv->treeview), column);
	gtk_tree_view_column_set_sort_column_id (column, APS_NAME_STR);
	gtk_tree_view_column_set_expand (column, TRUE);
	g_object_set (column, "resizable", TRUE, NULL);
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Count"), renderer,
	                                                   "text", APS_COUNT_STR,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (apd->priv->treeview), column);
	gtk_tree_view_column_set_sort_column_id (column, APS_COUNT);

	g_signal_connect_object (apd->priv->dialog, "destroy", G_CALLBACK (attention_profile_dialog_destroy_cb), apd, 0);

	gtk_widget_show_all (GTK_WIDGET (apd->priv->dialog));

	attention_profile_dialog_update (apd);

	apd->priv->updateTimeout = g_timeout_add_seconds (5, attention_profile_dialog_update, apd);

	return apd;
}

