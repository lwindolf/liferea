/**
 * @file rule.h feed/vfolder rule handling
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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

/** structure to store a rule instance */
typedef struct rule {
	feedPtr		fp;		/* the feed the filter is applied to */
	gchar		*value;		/* the value of the rule, e.g. a search text */
	gpointer	ruleInfo;	/* info structure about rule check function */
	gboolean	additive;	/* is the rule positive logic */
} *rulePtr;

/** rule info structure */
typedef struct ruleInfo {
	gpointer		ruleFunc;	/* the rules test function */
	gchar			*ruleId;	/* rule id for cache file storage */
	gchar			*title;		/* rule type title for dialogs */
	gchar			*positive;	/* text for positive logic selection */
	gchar			*negative;	/* text for negative logic selection */
	gboolean		needsParameter;	/* some rules may require no parameter... */
} *ruleInfoPtr;

/** the list of implemented rules */
extern struct ruleInfo *ruleFunctions;
extern gint nrOfRuleFunctions;

/** initializes the rule handling */
void rule_init(void);

/** 
 * Looks up the given rule id and sets up a new rule
 * structure with for the given vfolder and rule value 
 */
rulePtr rule_new(feedPtr fp, gchar *ruleId, gchar *value);

/**
 * Checks a new item against all additive rules of all feeds
 * except the addition rules of the parent feed. In the second
 * step the function checks wether there are parent feed rules,
 * which do exclude this item. If there is such a rule the 
 * function returns FALSE, otherwise TRUE to signalize if 
 * this new item should be added. 
 */
gboolean rule_check_item(rulePtr rp, itemPtr ip);

/**
 * Returns a title to be displayed in the filter editing dialog,
 * the returned title must be freed 
 */
gchar * rule_get_title(rulePtr rp);

/** Free's the given rule structure */
void rule_free(rulePtr rp);

#endif
