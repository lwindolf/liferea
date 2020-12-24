/**
 * @file rule.h  item matching rules used by search folders
 *
 * Copyright (C) 2003-2020 Lars Windolf <lars.windolf@gmx.de>
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
	const gchar	*ruleId;	/**< rule id for cache file storage */
	gchar		*title;		/**< rule type title for dialogs */
	gchar		*positive;	/**< text for positive logic selection */
	gchar		*negative;	/**< text for negative logic selection */
	gboolean	needsParameter;	/**< some rules may require no parameter... */

	gpointer	checkFunc;	/**< the item check function */
} *ruleInfoPtr;

/** structure to store a rule instance */
typedef struct rule {
	gchar		*value;			/* the value of the rule, e.g. a search text */
	gchar		*valueCaseFolded;	/* the value of the rule prepared by g_utf8_casefold() */
	ruleInfoPtr	ruleInfo;		/* info structure about rule check function */
	gboolean	additive;		/* is the rule positive logic */
} *rulePtr;

/** function type used to check items */
typedef gboolean (*ruleCheckFunc)	(rulePtr rule, itemPtr item);

/**
 * Returns a list of rule infos. To be used for rule editor
 * dialog setup.
 *
 * @returns a list of rule infos
 */
GSList * rule_get_available_rules (void);

/**
 * Looks up the given rule id and sets up a new rule
 * structure with for the given search folder and rule value
 *
 * @param ruleId	id string for this rule type
 * @param value		argument string for this rule
 * @param additive	indicates positive or negative logic
 *
 * @returns a new rule structure
 */
rulePtr rule_new (const gchar *ruleId, const gchar *value, gboolean additive);

/**
 * Setter for rule value
 *
 * @param rule		the rule
 * @param value		the new value
 */
void rule_set_value (rulePtr rule, const gchar *value);

/**
 * Free's the given rule structure
 *
 * @param rule	rule to free
 */
void rule_free (rulePtr rule);

#endif
