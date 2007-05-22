/**
 * @file rule.c DB based item matching rule handling
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h> /* For strstr() */

#include "common.h"
#include "db.h"
#include "debug.h"
#include "rule.h"

#define ITEM_MATCH_RULE_ID		"exact"
#define ITEM_TITLE_MATCH_RULE_ID	"exact_title"
#define ITEM_DESC_MATCH_RULE_ID		"exact_desc"
   
/** function type used to query SQL WHERE clause condition for rules */
typedef gchar * (*ruleConditionFunc)	(rulePtr rule);

/** function type used to check in memory items */
typedef gboolean (*ruleCheckFunc)	(rulePtr rule, itemPtr item);
   
/* the rule function list is used to create popup menues in ui_vfolder.c */
struct ruleInfo *ruleFunctions = NULL;
gint nrOfRuleFunctions = 0;

rulePtr
rule_new (struct vfolder *vfolder,
          const gchar *ruleId,
          const gchar *value,
          gboolean additive) 
{
	ruleInfoPtr	ruleInfo;
	rulePtr		rule;
	int		i;
	
	for (i = 0, ruleInfo = ruleFunctions; i < nrOfRuleFunctions; i++, ruleInfo++) 
	{
		if (0 == strcmp (ruleInfo->ruleId, ruleId)) 
		{
			rule = (rulePtr) g_new0 (struct rule, 1);
			rule->ruleInfo = ruleInfo;
			rule->additive = additive;
			rule->vp = vfolder;		
			rule->value = g_strdup (value);	
			return rule;
		}
	}	
	return NULL;
}

void 
rule_free (rulePtr rule)
{
	g_free (rule->value);
	g_free (rule);
}

/* -------------------------------------------------------------------- */
/* rule conditions							*/
/* -------------------------------------------------------------------- */

static gchar *
rule_condition_feed_title_match (rulePtr rule) 
{
	return g_strdup ("");	// FIXME: cannot be realized without having feeds in DB
}

static gchar *
rule_condition_item_title_match (rulePtr rule) 
{
	gchar	*result, *pattern;
	
	pattern = common_strreplace (g_strdup (rule->value), "'", "");
	result = g_strdup_printf ("items.title LIKE '%%%s%%'", pattern);
	g_free (pattern);
	
	return result;
}

static gchar *
rule_condition_item_description_match (rulePtr rule) 
{
	gchar	*result, *pattern;
	
	pattern = common_strreplace (g_strdup (rule->value), "'", "");
	result = g_strdup_printf ("items.description LIKE '%%%s%%'", pattern);
	g_free (pattern);
	
	return result;
}

static gchar *
rule_condition_item_match (rulePtr rule) 
{
	gchar	*result, *pattern;
	
	pattern = common_strreplace (g_strdup (rule->value), "'", "");
	result = g_strdup_printf ("(items.title LIKE '%%%s%%' AND items.description LIKE '%%%s%%')", pattern, pattern);
	g_free (pattern);
	
	return result;
}

static gchar *
rule_condition_item_is_unread (rulePtr rule) 
{
	return g_strdup ("items.read = 0");
}

static gboolean
rule_check_item_is_unread (rulePtr rule, itemPtr item)
{
	return (0 == item->readStatus);
}

static gchar *
rule_condition_item_is_flagged (rulePtr rule) 
{
	return g_strdup ("items.marked = 1");
}

static gboolean
rule_check_item_is_flagged (rulePtr rule, itemPtr item)
{
	return (1 == item->flagStatus);
}

static gchar *
rule_condition_item_was_updated (rulePtr rule)
{
	return g_strdup ("items.updated = 1");
}

static gchar *
rule_condition_item_has_enclosure (rulePtr rule) 
{
	return g_strdup ("metadata.key = 'enclosure'");
}

static queryPtr
query_create (GSList *rules)
{
	queryPtr	query;
	
	query = g_new0 (struct query, 1);
	while (rules) {
		rulePtr rule = (rulePtr)rules->data;
		query->tables |= rule->ruleInfo->queryTables;
		if (!query->conditions) {
			query->conditions = (*((ruleConditionFunc)rule->ruleInfo->queryFunc)) (rule);
		} else {
			gchar *old, *new;
			old = query->conditions;
			new = (*((ruleConditionFunc)rule->ruleInfo->queryFunc)) (rule);
			query->conditions = g_strdup_printf ("%s AND %s", old, new);
			g_free (new);
			g_free (old);
		}
		rules = g_slist_next (rules);
	}
	return query;
}

static void
query_free (queryPtr query)
{
	g_free (query->conditions);
	g_free (query);
}

void
rules_to_view (GSList *rules, const gchar *id)
{
	queryPtr	query;
	
	query = query_create (rules);
	db_view_create (id, query);
	query_free (query);
}

gboolean
rules_check_item (GSList *rules, itemPtr item)
{
	gboolean	result;
	queryPtr	query;
	
	/* first try in memory checks (for "unread" and "important" search folder)... */
	if (1 == g_slist_length (rules)) {
		rulePtr rule = (rulePtr) rules->data;
		ruleCheckFunc func = rule->ruleInfo->checkFunc;
		if (func) {
			result = (*func) (rules->data, item);
			return (rule->additive)?result:!result;
		}
	}

	/* if not possible query DB */
	query = query_create (rules);	
	result = db_item_check (item->id, query);
	query_free (query);
	
	return result;
}

/* rule initialization */

static void
rule_add (ruleConditionFunc queryFunc,
          ruleCheckFunc checkFunc,
          gchar *ruleId, 
          gchar *title,
          gchar *positive,
          gchar *negative,
          gboolean needsParameter,	/* has parameters */
	  guint queryTables)
{

	ruleFunctions = (ruleInfoPtr) g_realloc (ruleFunctions, sizeof (struct ruleInfo) * (nrOfRuleFunctions + 1));
	if (NULL == ruleFunctions)
		g_error("could not allocate memory!");
		
	ruleFunctions[nrOfRuleFunctions].ruleId = ruleId;
	ruleFunctions[nrOfRuleFunctions].title = title;
	ruleFunctions[nrOfRuleFunctions].positive = positive;
	ruleFunctions[nrOfRuleFunctions].negative = negative;
	ruleFunctions[nrOfRuleFunctions].needsParameter = needsParameter;
	
	ruleFunctions[nrOfRuleFunctions].checkFunc = checkFunc;

	ruleFunctions[nrOfRuleFunctions].queryFunc = queryFunc;	
	ruleFunctions[nrOfRuleFunctions].queryTables = queryTables;
	nrOfRuleFunctions++;
}

void
rule_init (void) 
{
	debug_enter ("rule_init");

	rule_add (rule_condition_item_match,		NULL,				ITEM_MATCH_RULE_ID,		_("Item"),		_("does contain"),	_("does not contain"),	TRUE, QUERY_TABLE_ITEMS);
	rule_add (rule_condition_item_title_match,	NULL,				ITEM_TITLE_MATCH_RULE_ID,	_("Item title"),	_("does match"),	_("does not match"),	TRUE, QUERY_TABLE_ITEMS);
	rule_add (rule_condition_item_description_match, NULL,				ITEM_DESC_MATCH_RULE_ID,	_("Item body"),		_("does match"),	_("does not match"),	TRUE, QUERY_TABLE_ITEMS);
	rule_add (rule_condition_feed_title_match,	NULL,				"feed_title",			_("Feed title"),	_("does match"),	_("does not match"),	TRUE, QUERY_TABLE_ITEMS);
	rule_add (rule_condition_item_is_unread,	rule_check_item_is_unread,	"unread",			_("Read status"),	_("is unread"),		_("is read"),		FALSE, QUERY_TABLE_ITEMS);
	rule_add (rule_condition_item_is_flagged,	rule_check_item_is_flagged,	"flagged",			_("Flag status"),	_("is flagged"),	_("is unflagged"),	FALSE, QUERY_TABLE_ITEMS);
	rule_add (rule_condition_item_was_updated,	NULL, 				"updated",			_("Update status"),	_("was updated"),	_("was not updated"),	FALSE, QUERY_TABLE_ITEMS);
	rule_add (rule_condition_item_has_enclosure,	NULL,				"enclosure",			_("Podcast"),		_("included"),		_("not included"),	FALSE, QUERY_TABLE_METADATA);

	debug_exit ("rule_init");
}
