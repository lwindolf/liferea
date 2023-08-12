/**
 * @file rule_editor.c  rule editing dialog functionality
 *
 * Copyright (C) 2008-2020 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2009 Hubert Figuiere <hub@figuiere.net>
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

#include "ui/rule_editor.h"
#include "ui/ui_common.h"

#include "common.h"
#include "rule.h"

/*
   A 'rule editor' is a dialog allowing editing arbitrary filtering
   'rules'. The rules edited are loaded from an 'item set' which
   can belong to a 'search folder' or an 'item list' filter.

   The rule editing is independant of any search folder handling.
*/

struct _RuleEditor {
	GObject		parentInstance;

	GtkWidget	*root;		/**< root widget */
	GSList		*newRules;	/**< new list of rules currently in editing */
};

struct changeRequest {
	GtkWidget	*hbox;		/**< used for remove button (optional) */
	RuleEditor	*editor;	/**< the rule editor */
	gint		rule;		/**< used for rule type change (optional) */
	GtkWidget	*paramHBox;	/**< used for rule type change (optional) */
};

G_DEFINE_TYPE (RuleEditor, rule_editor, G_TYPE_OBJECT);

static void
rule_editor_finalize (GObject *object)
{
	RuleEditor *re = RULE_EDITOR (object);

	/* delete rules */
	GSList *iter = re->newRules;
	while (iter) {
		rule_free ((rulePtr)iter->data);
		iter = g_slist_next (iter);
	}
	g_slist_free (re->newRules);
	re->newRules = NULL;
}

static void
rule_editor_class_init (RuleEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = rule_editor_finalize;
}

static void
rule_editor_init (RuleEditor *re)
{
}

static void
rule_editor_destroy_param_widget (GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(widget);
}

static void
on_rulevalue_changed (GtkEditable *editable, gpointer user_data)
{
	rule_set_value ((rulePtr)user_data, gtk_editable_get_chars (editable, 0, -1));
}

/* callback for rule additive option menu */

static void
on_rule_changed_additive (GtkComboBox *optionmenu, gpointer user_data)
{
	rulePtr rule = (rulePtr)user_data;
	gint active = gtk_combo_box_get_active (optionmenu);

	rule->additive = ((active==0) ? TRUE : FALSE);
}

/* sets up the widgets for a single rule */
static void
rule_editor_setup_widgets (struct changeRequest *changeRequest, rulePtr rule)
{
	GtkWidget	*widget;
	ruleInfoPtr	ruleInfo;

	ruleInfo = g_slist_nth_data (rule_get_available_rules (), changeRequest->rule);
	g_object_set_data (G_OBJECT (changeRequest->paramHBox), "rule", rule);

	/* remove of old widgets */
	gtk_container_foreach (GTK_CONTAINER (changeRequest->paramHBox), rule_editor_destroy_param_widget, NULL);

	/* add popup menu for selection of positive or negative logic */

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), ruleInfo->positive);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), ruleInfo->negative);
	gtk_combo_box_set_active ((GtkComboBox*)widget, (rule->additive)?0:1);
	g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (on_rule_changed_additive), rule);
	gtk_widget_show_all (widget);
	gtk_box_pack_start (GTK_BOX (changeRequest->paramHBox), widget, FALSE, FALSE, 0);

	/* add new value entry if needed */
	if (ruleInfo->needsParameter) {
		widget = gtk_entry_new ();
		gtk_entry_set_text (GTK_ENTRY (widget), rule->value);
		gtk_widget_show (widget);
		g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK(on_rulevalue_changed), rule);
		gtk_box_pack_start (GTK_BOX (changeRequest->paramHBox), widget, FALSE, FALSE, 0);
	} else {
		/* nothing needs to be added */
	}
}

static void
do_ruletype_changed (struct changeRequest	*changeRequest)
{
	ruleInfoPtr		ruleInfo;
	rulePtr			curRule, newRule;

	curRule = g_object_get_data (G_OBJECT (changeRequest->paramHBox), "rule");
	ruleInfo = g_slist_nth_data (rule_get_available_rules (), changeRequest->rule);
	newRule = rule_new (ruleInfo->ruleId, curRule && curRule->value ? curRule->value : "", TRUE);
	if (curRule) {
		changeRequest->editor->newRules = g_slist_remove (changeRequest->editor->newRules, curRule);
		rule_free (curRule);
	}
	changeRequest->editor->newRules = g_slist_append (changeRequest->editor->newRules, newRule);

	rule_editor_setup_widgets (changeRequest, newRule);
}

/* callback for rule type option menu */
static void
on_ruletype_changed (GtkComboBox *optionmenu, gpointer user_data)
{
	struct changeRequest	*changeRequest = NULL;
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (optionmenu, &iter)) {
		gtk_tree_model_get (gtk_combo_box_get_model (optionmenu), &iter, 1, &changeRequest, -1);
		if (changeRequest)
			do_ruletype_changed (changeRequest);
	}
}

/* callback for each rules remove button */
static void
on_ruleremove_clicked (GtkButton *button, gpointer user_data)
{
	struct changeRequest	*changeRequest = (struct changeRequest *)user_data;
	rulePtr			rule;

	rule = g_object_get_data (G_OBJECT (changeRequest->paramHBox), "rule");
	if (rule) {
		changeRequest->editor->newRules = g_slist_remove (changeRequest->editor->newRules, rule);
		rule_free(rule);
	}
	gtk_container_remove (GTK_CONTAINER (changeRequest->editor->root), changeRequest->hbox);
	g_free (changeRequest);
}

void
rule_editor_add_rule (RuleEditor *re, rulePtr rule)
{
	GSList			*ruleIter;
	GtkWidget		*hbox, *hbox2, *widget;
	GtkListStore		*store;
	struct changeRequest	*changeRequest, *selected = NULL;
	gint			i = 0, active = 0;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);	/* hbox to contain all rule widgets */
	hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);	/* another hbox where the rule specific widgets are added */

	/* set up the rule type selection popup */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	ruleIter = rule_get_available_rules ();
	while (ruleIter) {
		ruleInfoPtr ruleInfo = (ruleInfoPtr)ruleIter->data;
		GtkTreeIter iter;

		/* we add a change request to each popup option */
		changeRequest = g_new0 (struct changeRequest, 1);
		changeRequest->paramHBox = hbox2;
		changeRequest->rule = i;
		changeRequest->editor = re;

		if (0 == i)
			selected = changeRequest;

		/* build the menu option */
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, ruleInfo->title, 1, changeRequest, -1);

		if (rule) {
			if (ruleInfo == rule->ruleInfo) {
				selected = changeRequest;
				active = i;
			}
		}

		ruleIter = g_slist_next (ruleIter);
		i++;
	}
	widget = gtk_combo_box_new ();
	ui_common_setup_combo_text (GTK_COMBO_BOX (widget), 0);

	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);
	g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (on_ruletype_changed), NULL);


	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 0);

	if (!rule) {
		/* fake a rule type change to initialize parameter widgets */
		do_ruletype_changed (selected);
	} else {
		rulePtr newRule = rule_new (rule->ruleInfo->ruleId, rule->value, rule->additive);

		/* set up widgets with existing rule type and value */
		rule_editor_setup_widgets (selected, newRule);

		/* add the rule to the list of new rules */
		re->newRules = g_slist_append (re->newRules, newRule);
	}

	/* add remove button */
	changeRequest = g_new0 (struct changeRequest, 1);
	changeRequest->hbox = hbox;
	changeRequest->paramHBox = hbox2;
	changeRequest->editor = re;
	widget = gtk_button_new_with_label (_("Remove"));
	gtk_box_pack_end (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (on_ruleremove_clicked), changeRequest);

	/* and insert everything in the dialog */
	gtk_widget_show_all (hbox);
	gtk_box_pack_start (GTK_BOX (re->root), hbox, FALSE, TRUE, 0);
}

RuleEditor *
rule_editor_new (itemSetPtr itemset)
{
	RuleEditor	*re;
	GSList		*iter;

	re = RULE_EDITOR (g_object_new (RULE_EDITOR_TYPE, NULL));

	/* Set up rule list vbox */
	re->root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	/* load rules into dialog */
	iter = itemset->rules;
	while (iter) {
		rule_editor_add_rule (re, (rulePtr)(iter->data));
		iter = g_slist_next (iter);
	}

	gtk_widget_show_all (re->root);

	return re;
}

void
rule_editor_save (RuleEditor *re, itemSetPtr itemset)
{
	GSList	*iter;

	/* delete all old rules */
	iter = itemset->rules;
	while (iter) {
		rule_free ((rulePtr)iter->data);
		iter = g_slist_next (iter);
	}
	g_slist_free (itemset->rules);
	itemset->rules = NULL;

	/* and add all rules from editor */
	iter = re->newRules;
	while (iter) {
		rulePtr rule = (rulePtr)iter->data;
		itemset_add_rule (itemset, rule->ruleInfo->ruleId, rule->value, rule->additive);
		iter = g_slist_next (iter);
	}
}

GtkWidget *
rule_editor_get_widget (RuleEditor *re)
{
	return re->root;
}
