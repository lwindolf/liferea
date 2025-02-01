/**
 * @file search_dialog.c  Search engine subscription dialog
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

#include "ui/search_dialog.h"

#include <string.h>

#include "common.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "itemlist.h"
#include "node.h"
#include "node_view.h"
#include "rule.h"
#include "node_providers/vfolder.h"
#include "vfolder_loader.h"
#include "ui/item_list_view.h"
#include "ui/liferea_dialog.h"
#include "ui/rule_editor.h"

/* a single static search folder representing the active search dialog result */
static vfolderPtr vfolder = NULL;

/* shared functions */

static void
search_clean_results (vfolderPtr vfolder)
{
	if (!vfolder)
		return;

	/* Clean up old search result data and display... */
	if (vfolder->node == itemlist_get_displayed_node ())
		itemlist_unload ();

	/* FIXME: Don't simply free the result search folder
	   as the search query might still be active. Instead
	   g_object_unref() a search result object! For now
	   we leak the node to avoid crashes. */
	//g_object_unref (vfolder->node);
}

static void
search_load_results (vfolderPtr searchResult)
{
	feedlist_set_selected (NULL);

	itemlist_add_search_result (vfolder_loader_new (searchResult->node));
}

/* complex search dialog */

static GtkWidget *search_dialog = NULL;

static void
on_search_dialog_response (GtkDialog *dialog, gint responseId, gpointer user_data)
{
	if (1 == responseId) { /* Search */
		search_clean_results (vfolder);

		vfolder = vfolder_new (node_new ("vfolder"));
		rule_editor_save (RULE_EDITOR (user_data), vfolder->itemset);
		vfolder->itemset->anyMatch = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "anyRuleRadioBtn2")));

		search_load_results (vfolder);
	}

	if (2 == responseId) { /* Create Search Folder */
		rule_editor_save (RULE_EDITOR (user_data), vfolder->itemset);
		vfolder->itemset->anyMatch = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (GTK_WIDGET (dialog), "anyRuleRadioBtn2")));

		Node *node = vfolder->node;
		vfolder = NULL;
		feedlist_node_added (node);
	}

	if (1 != responseId) {
		search_clean_results (vfolder);
		search_dialog = NULL;
	}
}

/* callback copied from search_folder_dialog.c */
static void
on_addrulebtn_clicked (GtkButton *button, gpointer user_data)
{
	rule_editor_add_rule (RULE_EDITOR (user_data), NULL);
}

void
search_dialog_open (const gchar *query)
{
	static GtkWidget *dialog;
	RuleEditor *re;

	if (search_dialog)
		return;

	search_dialog = dialog = liferea_dialog_new ("search");
	vfolder = vfolder_new (node_new ("vfolder"));
	node_set_title (vfolder->node, _("Saved Search"));

	if (query)
		itemset_add_rule (vfolder->itemset, "exact", query, TRUE);

	re = rule_editor_new (vfolder->itemset);

	/* Note: the following code is somewhat duplicated from search_folder_dialog.c */

	/* Setting default rule match type */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (liferea_dialog_lookup (dialog, "anyRuleRadioBtn2")), TRUE);

	/* Set up rule list vbox */
	gtk_viewport_set_child (GTK_VIEWPORT (liferea_dialog_lookup (dialog, "ruleview_search_dialog")), rule_editor_get_widget (re));

	/* bind buttons */
	g_signal_connect (liferea_dialog_lookup (dialog, "addrulebtn2"), "clicked", G_CALLBACK (on_addrulebtn_clicked), re);
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (on_search_dialog_response), re);
}

/* simple search dialog */

static GtkWidget *simple_dialog = NULL;

static void
on_simple_search_dialog_response (GtkDialog *dialog, gint responseId, gpointer user_data)
{
	const gchar	*searchString = liferea_dialog_entry_get (GTK_WIDGET (dialog), "searchentry");

	if (1 == responseId) {	/* Search */
		search_clean_results (vfolder);

		/* Create new search... */
		vfolder = vfolder_new (node_new ("vfolder"));
		node_set_title (vfolder->node, searchString);
		itemset_add_rule (vfolder->itemset, "exact", searchString, TRUE);

		search_load_results (vfolder);
	}

	if (2 == responseId)	/* Advanced... */
		search_dialog_open (searchString);

	/* Do not close the dialog when "just" searching. The user
	   should click "Close" to close the dialog to be able to
	   do subsequent searches... */
	if (1 != responseId) {
		search_clean_results (vfolder);
		simple_dialog = NULL;
	}
}

static void
on_searchentry_activated (GtkEntry *entry, gpointer user_data)
{
	/* simulate search response */
	on_simple_search_dialog_response (GTK_DIALOG (user_data), 1, NULL);
}

static void
on_searchentry_changed (GtkEditable *editable, gpointer user_data)
{
	gchar 		*searchString;

	/* just to disable the start search button when search string is empty... */
	searchString = gtk_editable_get_chars (editable, 0, -1);
	gtk_widget_set_sensitive (liferea_dialog_lookup (simple_dialog, "searchstartbtn"), searchString && (0 < strlen (searchString)));
}

void 
simple_search_dialog_open (void)
{
	GtkWidget *dialog;

	if (simple_dialog)
		return;

	simple_dialog = dialog = liferea_dialog_new ("simple_search");

	gtk_window_set_focus (GTK_WINDOW (dialog), liferea_dialog_lookup (dialog, "searchentry"));

	g_signal_connect (dialog, "response", G_CALLBACK (on_simple_search_dialog_response), NULL);
	g_signal_connect (liferea_dialog_lookup (dialog, "searchentry"), "changed", G_CALLBACK (on_searchentry_changed), dialog);
	g_signal_connect (liferea_dialog_lookup (dialog, "searchentry"), "activate", G_CALLBACK (on_searchentry_activated), dialog);
}