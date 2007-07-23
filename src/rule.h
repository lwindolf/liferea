/**
 * @file rule.h DB item match rule handling
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _RULE_H
#define _RULE_H

#include <glib.h>
#include "item.h"

/** rule info structure */
typedef struct ruleInfo {
	gchar		*ruleId;	/**< rule id for cache file storage */
	gchar		*title;		/**< rule type title for dialogs */
	gchar		*positive;	/**< text for positive logic selection */
	gchar		*negative;	/**< text for negative logic selection */
	gboolean	needsParameter;	/**< some rules may require no parameter... */
	
	gpointer	checkFunc;	/**< the in memory check function (or NULL) */
	
	gpointer	queryFunc;	/**< the query condition creation function */
	guint		queryTables;	/**< tables necessary for the rule query */
} *ruleInfoPtr;

/** structure to store a rule instance */
typedef struct rule {
	struct vfolder	*vp;		/* the vfolder the rule belongs to */
	gchar		*value;		/* the value of the rule, e.g. a search text */
	ruleInfoPtr	ruleInfo;	/* info structure about rule check function */
	gboolean	additive;	/* is the rule positive logic */
} *rulePtr;

/** the list of implemented rules */
extern struct ruleInfo *ruleFunctions;
extern gint nrOfRuleFunctions;

/**
 * Initializes the rule handling 
 */
void rule_init (void);

/** 
 * Looks up the given rule id and sets up a new rule
 * structure with for the given search folder and rule value 
 *
 * @param vfolder	search folder the rule belongs to
 * @param ruleId	id string for this rule type
 * @param value		argument string for this rule
 * @param additive	indicates positive or negative logic
 *
 * @returns a new rule structure
 */
rulePtr rule_new (struct vfolder *vfolder, const gchar *ruleId, const gchar *value, gboolean additive);

/**
 * Processes the given rule list and creates a DB view
 * with the given id.
 *
 * @param rules		the rule list
 * @param id		the view id
 */
void rules_to_view (GSList *rules, const gchar *id);

/**
 * Checks if the given item id matches the given rules.
 *
 * @param rules		the rule list
 * @param item		the item
 *
 * @returns TRUE if the item matches the rules
 */
gboolean rules_check_item (GSList *rules, itemPtr item);

/** 
 * Free's the given rule structure 
 *
 * @param rule	rule to free
 */
void rule_free (rulePtr rule);

#endif
