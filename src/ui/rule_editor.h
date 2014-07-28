/**
 * @file rule_editor.h  rule editing dialog functionality
 *
 * Copyright (C) 2008-2011 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _RULE_EDITOR_H
#define _RULE_EDITOR_H

#include <gtk/gtk.h>
#include <glib-object.h>
#include <glib.h>

#include "rule.h"
#include "itemset.h"

G_BEGIN_DECLS

#define RULE_EDITOR_TYPE		(rule_editor_get_type ())
#define RULE_EDITOR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), RULE_EDITOR_TYPE, RuleEditor))
#define RULE_EDITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), RULE_EDITOR_TYPE, RuleEditorClass))
#define IS_RULE_EDITOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), RULE_EDITOR_TYPE))
#define IS_RULE_EDITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), RULE_EDITOR_TYPE))

typedef struct RuleEditor		RuleEditor;
typedef struct RuleEditorClass		RuleEditorClass;
typedef struct RuleEditorPrivate	RuleEditorPrivate;

struct RuleEditor
{
	GObject		parent;
	
	/*< private >*/
	RuleEditorPrivate	*priv;
};

struct RuleEditorClass 
{
	GObjectClass parent_class;
};

GType rule_editor_get_type	(void);

/**
 * Create a new rule editor widget set. Loads all rules
 * of the itemset passed.
 *
 * @param itemset	the itemset with the rules to load
 *
 * @returns a new RuleEditor instance
 */
RuleEditor * rule_editor_new (itemSetPtr itemset);

/**
 * This method is used to add another rule to a rule editor. 
 * The rule parameter might be NULL to create new rules or a 
 * pointer to an existing rule to load it into the dialog.
 *
 * @param re		the rule editor
 * @param rule		the rule to add
 */
void rule_editor_add_rule (RuleEditor *re, rulePtr rule);

/**
 * Saves the editing state of the rule editor to
 * the given search folder.
 *
 * @param re		the rule editor
 * @param itemset	the item set to set the rules to
 */
void rule_editor_save (RuleEditor *re, itemSetPtr itemset);

/**
 * Get the root widget of the rule editor.
 *
 * @param re		the rule editor
 *
 * @returns the root widget
 */
GtkWidget * rule_editor_get_widget (RuleEditor *re);

G_END_DECLS

#endif
