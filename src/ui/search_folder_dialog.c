/**
 * @file search-folder-dialog.c  Search folder properties dialog
 *
 * Copyright (C) 2007-2008 Lars Lindner <lars.lindner@gmail.com>
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

#include "ui/search_folder_dialog.h"

#include <gtk/gtk.h>

#include "vfolder.h"
#include "ui/rule_editor.h"
#include "ui/ui_dialog.h"
#include "ui/ui_shell.h"

static void search_folder_dialog_class_init	(SearchFolderDialogClass *klass);
static void search_folder_dialog_init		(SearchFolderDialog *ld);

#define SEARCH_FOLDER_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), SEARCH_FOLDER_DIALOG_TYPE, SearchFolderDialogPrivate))

struct SearchFolderDialogPrivate {
	RuleEditor	*re;		/**< dynamically created rule editing widget subset */
	GtkWidget	*nameEntry;	/**< search folder title entry */
	nodePtr		node;		/**< search folder feed list node */
	vfolderPtr	vfolder;	/**< the search folder */
};

static GObjectClass *parent_class = NULL;

GType
search_folder_dialog_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (SearchFolderDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) search_folder_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (SearchFolderDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) search_folder_dialog_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "SearchFolderDialog",
					       &our_info, 0);
	}

	return type;
}

static void
search_folder_dialog_finalize (GObject *object)
{
	SearchFolderDialog *sfd = SEARCH_FOLDER_DIALOG (object);
	
	g_object_unref (sfd->priv->re);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
search_folder_dialog_class_init (SearchFolderDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = search_folder_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(SearchFolderDialogPrivate));
}

static void
search_folder_dialog_init (SearchFolderDialog *sfd)
{
	sfd->priv = SEARCH_FOLDER_DIALOG_GET_PRIVATE (sfd);
}

static void
on_propdialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	SearchFolderDialog	*sfd = SEARCH_FOLDER_DIALOG (user_data);
	
	if (response_id == GTK_RESPONSE_OK) {	
		/* save new search folder settings */
		node_set_title (sfd->priv->node, gtk_entry_get_text (GTK_ENTRY (sfd->priv->nameEntry)));
		rule_editor_save (sfd->priv->re, sfd->priv->vfolder);
		sfd->priv->vfolder->anyMatch = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "anyRuleRadioBtn")));

		/* update search folder */
		ui_itemlist_clear ();
		vfolder_refresh (sfd->priv->vfolder);
		itemlist_unload (FALSE);
		itemlist_load (sfd->priv->node);
		ui_node_update (sfd->priv->node->id);
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_addrulebtn_clicked (GtkButton *button, gpointer user_data)
{
	SearchFolderDialog *sfd = SEARCH_FOLDER_DIALOG (user_data);
		
	rule_editor_add_rule (sfd->priv->re, NULL);
}

SearchFolderDialog *
search_folder_dialog_new (nodePtr node) 
{
	GtkWidget		*dialog;
	SearchFolderDialog	*sfd;
	RuleEditor		*re;
	
	sfd = SEARCH_FOLDER_DIALOG (g_object_new (SEARCH_FOLDER_DIALOG_TYPE, NULL));
	sfd->priv->re = rule_editor_new ((vfolderPtr)node->data);
	sfd->priv->node = node;
	sfd->priv->vfolder = (vfolderPtr)node->data;
	
	/* Create the dialog */
	dialog = liferea_dialog_new (NULL, "vfolderdialog");
	
	/* Setup search folder name */
	sfd->priv->nameEntry = liferea_dialog_lookup (dialog, "feedNameEntry");
	gtk_entry_set_text (GTK_ENTRY (sfd->priv->nameEntry), node_get_title (node));
	
	/* Set up rule match type */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, sfd->priv->vfolder->anyMatch?"anyRuleRadioBtn":"allRuleRadioBtn")), TRUE);
	
	/* Set up rule list vbox */
	gtk_container_add (GTK_CONTAINER (liferea_dialog_lookup (dialog, "ruleview_vfolder_dialog")), rule_editor_get_widget (sfd->priv->re));

	/* bind buttons */
	g_signal_connect (liferea_dialog_lookup (dialog, "addrulebtn"), "clicked", G_CALLBACK (on_addrulebtn_clicked), sfd);
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (on_propdialog_response), sfd);
	
	return sfd;
}
