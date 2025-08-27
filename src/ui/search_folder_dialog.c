/**
 * @file search-folder-dialog.c  Search folder properties dialog
 *
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "ui/search_folder_dialog.h"

#include "feedlist.h"
#include "itemlist.h"
#include "node_providers/vfolder.h"
#include "ui/feed_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/rule_editor.h"

struct _SearchFolderDialog {
	GObject		parentInstance;

	GtkWidget	*dialog;	/**< dialog widget */
	RuleEditor	*re;		/**< dynamically created rule editing widget subset */
	Node		*node;		/**< search folder feed list node */
	vfolderPtr	vfolder;	/**< the search folder */
};

G_DEFINE_TYPE (SearchFolderDialog, search_folder_dialog, G_TYPE_OBJECT);

static void
search_folder_dialog_finalize (GObject *object)
{
	SearchFolderDialog *sfd = SEARCH_FOLDER_DIALOG (object);

	g_object_unref (sfd->re);
}

static void
search_folder_dialog_class_init (SearchFolderDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = search_folder_dialog_finalize;
}

static void
search_folder_dialog_init (SearchFolderDialog *sfd)
{
}

static void
on_propdialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	SearchFolderDialog	*sfd = SEARCH_FOLDER_DIALOG (user_data);

	if (response_id == GTK_RESPONSE_OK) {
		/* save new search folder settings */
		node_set_title (sfd->node, liferea_dialog_entry_get (sfd->dialog, "searchNameEntry"));
		rule_editor_save (sfd->re, sfd->vfolder->itemset);
		sfd->vfolder->itemset->anyMatch = gtk_check_button_get_active (GTK_CHECK_BUTTON (liferea_dialog_lookup (sfd->dialog, "anyRuleRadioBtn")));
		sfd->vfolder->unreadOnly = gtk_check_button_get_active (GTK_CHECK_BUTTON (liferea_dialog_lookup (sfd->dialog, "hideReadItemsBtn")));

		/* update search folder */
		vfolder_reset (sfd->vfolder);
		itemlist_unload ();

		/* If we are finished editing a new search folder add it to the feed list */
		if (!sfd->node->parent)
			feedlist_node_added (sfd->node);

		feedlist_node_was_updated (sfd->node);

		/* rebuild the search folder */
		vfolder_rebuild (sfd->node);
	}

	gtk_window_close (GTK_WINDOW (sfd->dialog));
}

static int
scroll_down (GtkScrolledWindow *scrolled_window)
{
	GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment (scrolled_window);
	gtk_adjustment_set_value (adjustment, gtk_adjustment_get_upper (adjustment));

	return FALSE;
}

static void
on_addrulebtn_clicked (GtkButton *button, gpointer user_data)
{
	SearchFolderDialog *sfd = SEARCH_FOLDER_DIALOG (user_data);

	rule_editor_add_rule (sfd->re, NULL);

	g_idle_add ((GSourceFunc)scroll_down, liferea_dialog_lookup (sfd->dialog, "ruleview_scrolled_window"));
}

/** Use to create new search folders and to edit existing ones */
SearchFolderDialog *
search_folder_dialog_new (Node *node)
{
	SearchFolderDialog	*sfd;

	sfd = SEARCH_FOLDER_DIALOG (g_object_new (SEARCH_FOLDER_DIALOG_TYPE, NULL));
	sfd->node = node;
	sfd->vfolder = (vfolderPtr)node->data;
	sfd->re = rule_editor_new (sfd->vfolder->itemset);
	sfd->dialog = liferea_dialog_new ("search_folder");

	liferea_dialog_entry_set (sfd->dialog, "searchNameEntry", node_get_title (node));

	/* Set up rule match type */
	gtk_check_button_set_active (GTK_CHECK_BUTTON (liferea_dialog_lookup (sfd->dialog, sfd->vfolder->itemset->anyMatch?"anyRuleRadioBtn":"allRuleRadioBtn")), TRUE);
	gtk_check_button_set_active (GTK_CHECK_BUTTON (liferea_dialog_lookup (sfd->dialog, "hideReadItemsBtn")), sfd->vfolder->unreadOnly);

	/* Set up rule list vbox */
	gtk_viewport_set_child (GTK_VIEWPORT (liferea_dialog_lookup (sfd->dialog, "ruleview_vfolder_dialog")), rule_editor_get_widget (sfd->re));

	/* bind buttons */
	g_signal_connect (liferea_dialog_lookup (sfd->dialog, "addrulebtn"), "clicked", G_CALLBACK (on_addrulebtn_clicked), sfd);
	g_signal_connect (G_OBJECT (sfd->dialog), "response", G_CALLBACK (on_propdialog_response), sfd);

	return sfd;
}
