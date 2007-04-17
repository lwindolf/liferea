/**
 * @file rule.c feed/vfolder rule handling
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
#include "debug.h"
#include "feed.h"
#include "item.h"
#include "metadata.h"
#include "rule.h"
#include "support.h"

#define ITEM_MATCH_RULE_ID		"exact"
#define ITEM_TITLE_MATCH_RULE_ID	"exact_title"
#define ITEM_DESC_MATCH_RULE_ID		"exact_desc"
   
/** function type used to query SQL WHERE clause condition for rules */
typedef gchar * (*ruleConditionFunc)	(rulePtr rule);
   
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
rule_feed_title_match (rulePtr rule) 
{
	return g_strdup ("");	// FIXME: cannot be realized without having feeds in DB
}

static gchar *
rule_item_title_match (rulePtr rule) 
{
	gchar	*result, *pattern;
	
	pattern = common_strreplace (g_strdup (rule->value), "'", "");
	result = g_strdup_printf ("item.title LIKE '%s'", pattern);
	g_free (pattern);
	
	return result;
}

static gchar *
rule_item_description_match (rulePtr rule) 
{
	gchar	*result, *pattern;
	
	pattern = common_strreplace (g_strdup (rule->value), "'", "");
	result = g_strdup_printf ("item.description LIKE '%s'", pattern);
	g_free (pattern);
	
	return result;
}

static gchar *
rule_item_match (rulePtr rule) 
{
	gchar	*result, *pattern;
	
	pattern = common_strreplace (g_strdup (rule->value), "'", "");
	result = g_strdup_printf ("(item.title LIKE '%s' item.description LIKE '%s')", pattern, pattern);
	g_free (pattern);
	
	return result;
}

static gchar *
rule_item_is_unread (rulePtr rule) 
{
	return g_strdup ("items.read = 0");
}

static gchar *
rule_item_is_flagged (rulePtr rule) 
{
	return g_strdup ("items.marked = 1");
}

static gchar *
rule_item_was_updated (rulePtr rule)
{
	return g_strdup ("items.updated = 1");
}

static gchar *
rule_item_has_enclosure (rulePtr rule) 
{
	return g_strdup ("metadata.key = 'enclosure'");
}

gchar *
rules_to_sql_condition (GSList *rules)
{
	// FIXME
	return NULL;
}

/* rule initialization */

static void
rule_add (ruleConditionFunc func, 
          gchar *ruleId, 
          gchar *title,
          gchar *positive,
          gchar *negative,
          gboolean needsParameter,	/* has parameters */
	  gboolean itemMatch, 		/* needs items table */
	  gboolean metadataMatch)	/* needs metadata table */ 
{

	ruleFunctions = (ruleInfoPtr) g_realloc (ruleFunctions, sizeof (struct ruleInfo) * (nrOfRuleFunctions + 1));
	if (NULL == ruleFunctions)
		g_error("could not allocate memory!");
	ruleFunctions[nrOfRuleFunctions].ruleFunc = func;
	ruleFunctions[nrOfRuleFunctions].ruleId = ruleId;
	ruleFunctions[nrOfRuleFunctions].title = title;
	ruleFunctions[nrOfRuleFunctions].positive = positive;
	ruleFunctions[nrOfRuleFunctions].negative = negative;
	ruleFunctions[nrOfRuleFunctions].needsParameter = needsParameter;
	ruleFunctions[nrOfRuleFunctions].itemMatch = itemMatch;
	ruleFunctions[nrOfRuleFunctions].metadataMatch = metadataMatch;
	nrOfRuleFunctions++;
}

void
rule_init (void) 
{
	debug_enter ("rule_init");

	rule_add (rule_item_match,		ITEM_MATCH_RULE_ID,		_("Item"),		_("does contain"),	_("does not contain"),	TRUE, TRUE, FALSE);
	rule_add (rule_item_title_match,	ITEM_TITLE_MATCH_RULE_ID,	_("Item title"),	_("does match"),	_("does not match"),	TRUE, TRUE, FALSE);
	rule_add (rule_item_description_match,	ITEM_DESC_MATCH_RULE_ID,	_("Item body"),		_("does match"),	_("does not match"),	TRUE, TRUE, FALSE);
	rule_add (rule_feed_title_match,	"feed_title",			_("Feed title"),	_("does match"),	_("does not match"),	TRUE, TRUE, FALSE);
	rule_add (rule_item_is_unread,		"unread",			_("Read status"),	_("is unread"),		_("is read"),		FALSE, TRUE, FALSE);
	rule_add (rule_item_is_flagged,		"flagged",			_("Flag status"),	_("is flagged"),	_("is unflagged"),	FALSE, TRUE, FALSE);
	rule_add (rule_item_was_updated,	"updated",			_("Update status"),	_("was updated"),	_("was not updated"),	FALSE, TRUE, FALSE);
	rule_add (rule_item_has_enclosure,	"enclosure",			_("Podcast"),		_("included"),		_("not included"),	FALSE, FALSE, TRUE);

	debug_exit ("rule_init");
}
