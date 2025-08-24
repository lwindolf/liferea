/**
 * @file rule_editor.c  rule editing dialog
 *
 * Copyright (C) 2008-2025 Lars Windolf <lars.windolf@gmx.de>
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
   A 'rule editor' is an embeddable widget group allowing editing 
   arbitrary filtering 'rules'. The rules edited are loaded from an 
   'item set' which can belong to a 'search folder' or an 'item list' filter.

   The rule editing is independant of any search folder handling.
*/

struct _RuleEditor {
	GObject		parentInstance;

	GtkWidget	*root;		/**< root widget */
};

G_DEFINE_TYPE (RuleEditor, rule_editor, G_TYPE_OBJECT);

static void
rule_editor_class_init (RuleEditorClass *klass)
{
}

static void
rule_editor_init (RuleEditor *re)
{
}

static void
on_rulevalue_changed (GtkEditable *editable, gpointer user_data)
{
	rule_set_value ((rulePtr)user_data, gtk_editable_get_chars (editable, 0, -1));
}

static void
on_rule_changed_additive (GtkComboBox *optionmenu, gpointer user_data)
{
	rulePtr rule = (rulePtr)user_data;
	gint active = gtk_combo_box_get_active (optionmenu);

	rule->additive = ((active==0) ? TRUE : FALSE);
}

/**
 * Sets up the widgets for a single rule (called on add and change) 
 * Also persists the rule with g_object_set_data_full on the container
 * so it can be retrieved later.
 * 
 * @param container the outer row container
 * @param hbox the hbox containing the rule widgets to update
 */
static void
rule_editor_update_widgets (GtkWidget *hbox, rulePtr rule)
{
	GtkWidget 	*container = gtk_widget_get_parent (hbox);
	GtkWidget	*widget;

	/* rule infos are kept as container data */
	g_object_set_data_full (G_OBJECT (container), "rule", rule, (GDestroyNotify)rule_free);

	/* remove old widgets */
	widget = gtk_widget_get_first_child (hbox);
	while (widget) {
		GtkWidget *next = gtk_widget_get_next_sibling (widget);
		gtk_box_remove (GTK_BOX (hbox), widget);
		widget = next;
	}

	/* add popup menu for selection of positive or negative logic */
	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), rule->ruleInfo->positive);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), rule->ruleInfo->negative);
	gtk_combo_box_set_active ((GtkComboBox*)widget, (rule->additive)?0:1);
	g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (on_rule_changed_additive), rule);
	gtk_box_append (GTK_BOX (hbox), widget);

	/* add new value entry if needed */
	if (rule->ruleInfo->needsParameter) {
		g_autoptr(GtkEntryBuffer) buffer = gtk_entry_buffer_new (rule->value, -1);

		widget = gtk_entry_new ();
		gtk_entry_set_buffer (GTK_ENTRY (widget), buffer);
		g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (on_rulevalue_changed), rule);
		gtk_box_append (GTK_BOX (hbox), widget);
	}
}

static void
on_ruletype_changed (GtkDropDown *optionmenu, gpointer unused)
{
	ruleInfoPtr	ruleInfo;
	rulePtr		curRule, newRule;
	gint 		selected = gtk_drop_down_get_selected (optionmenu);

	curRule = (rulePtr)g_object_get_data (G_OBJECT (gtk_widget_get_parent (GTK_WIDGET (optionmenu))), "rule");
	ruleInfo = g_slist_nth_data (rule_get_available_rules (), selected);

	g_assert (curRule);
	g_assert (ruleInfo);

	newRule = rule_new (ruleInfo->ruleId, curRule && curRule->value ? curRule->value : "", TRUE);
	rule_editor_update_widgets (gtk_widget_get_next_sibling (GTK_WIDGET (optionmenu)), newRule);
}

static void
on_ruleremove_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget *container = GTK_WIDGET (user_data);

	gtk_box_remove (GTK_BOX (gtk_widget_get_parent (container)), container);
}

void
rule_editor_add_rule (RuleEditor *re, rulePtr rule)
{
	GSList			*ruleIter;
	GtkWidget		*container, *hbox, *widget;
	gint			ruleTypeNr = 0, selected = 0;
	rulePtr			newRule;

	container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);	/* row container with type select and remove button */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);		/* rule hbox where the rule specific widgets are added */

	/* set up the rule type selection popup */
	GtkStringList *list = gtk_string_list_new (NULL);
	ruleIter = rule_get_available_rules ();
	while (ruleIter) {
		ruleInfoPtr ruleInfo = (ruleInfoPtr)ruleIter->data;

		gtk_string_list_append (list, ruleInfo->title);
		
		if (rule && (ruleInfo == rule->ruleInfo))
			selected = ruleTypeNr;
		
		ruleIter = g_slist_next (ruleIter);
		ruleTypeNr++;
	}
	
	widget = gtk_drop_down_new (G_LIST_MODEL (list), NULL);
	gtk_drop_down_set_selected (GTK_DROP_DOWN (widget), selected);
	g_signal_connect (G_OBJECT (widget), "notify::selected", G_CALLBACK (on_ruletype_changed), NULL);

	gtk_box_append (GTK_BOX (container), widget);
	gtk_box_append (GTK_BOX (container), hbox);

	if (!rule)
		newRule = rule_new (((ruleInfoPtr)rule_get_available_rules()->data)->ruleId, "", TRUE);
	else
		/* it is important to create a rule copy here, as the "rule" passed might belong to an active search folder */
		newRule = rule_new (rule->ruleInfo->ruleId, rule->value, rule->additive);

	rule_editor_update_widgets (hbox, newRule);
	
	/* add remove button */
	widget = gtk_button_new_with_label (_("Remove"));
	gtk_box_append (GTK_BOX (container), widget);
	g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (on_ruleremove_clicked), container);

	gtk_box_append (GTK_BOX (re->root), container);
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
	GtkWidget *child = gtk_widget_get_first_child (re->root);
	while (child) {
		rulePtr rule = g_object_get_data (G_OBJECT (child), "rule");
		itemset_add_rule (itemset, rule->ruleInfo->ruleId, rule->value, rule->additive);
		child = gtk_widget_get_next_sibling (child);
	}
}

GtkWidget *
rule_editor_get_widget (RuleEditor *re)
{
	return re->root;
}
