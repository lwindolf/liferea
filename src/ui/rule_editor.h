/**
 * @file rule_editor.h  rule editing dialog functionality
 *
 * Copyright (C) 2008-2018 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _RULE_EDITOR_H
#define _RULE_EDITOR_H

#include <gtk/gtk.h>

#include "rule.h"
#include "itemset.h"

G_BEGIN_DECLS

#define RULE_EDITOR_TYPE		(rule_editor_get_type ())
G_DECLARE_FINAL_TYPE (RuleEditor, rule_editor, RULE, EDITOR, GObject)

/**
 * rule_editor_new:
 * Create a new rule editor widget set. Loads all rules
 * of the itemset passed.
 *
 * @itemset:	the itemset with the rules to load
 *
 * Returns: (transfer none): a new RuleEditor instance
 */
RuleEditor * rule_editor_new (itemSetPtr itemset);

/**
 * rule_editor_add_rule:
 * This method is used to add another rule to a rule editor.
 * The rule parameter might be NULL to create new rules or a
 * pointer to an existing rule to load it into the dialog.
 *
 * @re:			the rule editor
 * @rule:		the rule to add
 */
void rule_editor_add_rule (RuleEditor *re, rulePtr rule);

/**
 * rule_editor_save:
 * Saves the editing state of the rule editor to
 * the given search folder.
 *
 * @re:			the rule editor
 * @itemset:	the item set to set the rules to
 */
void rule_editor_save (RuleEditor *re, itemSetPtr itemset);

/**
 * rule_editor_get_widget:
 * Get the root widget of the rule editor.
 *
 * @re:		the rule editor
 *
 * Returns: (transfer none): the root widget
 */
GtkWidget * rule_editor_get_widget (RuleEditor *re);

G_END_DECLS

#endif
