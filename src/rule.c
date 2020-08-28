/**
 * @file rule.c  item matching rules used by search folders
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

#include "rule.h"

#include <string.h>

#include "common.h"
#include "debug.h"
#include "metadata.h"

#define ITEM_MATCH_RULE_ID		"exact"
#define ITEM_TITLE_MATCH_RULE_ID	"exact_title"
#define ITEM_DESC_MATCH_RULE_ID		"exact_desc"
#define FEED_TITLE_MATCH_RULE_ID	"feed_title"
#define FEED_SOURCE_MATCH_RULE_ID	"feed_source"
#define PARENT_FOLDER_MATCH_RULE_ID	"parent_folder"

/** list of available search folder rules */
static GSList *ruleFunctions = NULL;

static void rule_init (void);

GSList *
rule_get_available_rules (void)
{
	if (!ruleFunctions)
		rule_init ();

	return ruleFunctions;
}

/* rule creation */

rulePtr
rule_new (const gchar *ruleId,
          const gchar *value,
          gboolean additive)
{
	GSList		*iter;

	iter = rule_get_available_rules ();
	while (iter) {
		ruleInfoPtr ruleInfo = (ruleInfoPtr)iter->data;
		if (0 == strcmp (ruleInfo->ruleId, ruleId)) {
			rulePtr rule = (rulePtr) g_new0 (struct rule, 1);
			rule->ruleInfo = ruleInfo;
			rule->additive = additive;
			rule->value = common_strreplace (g_strdup (value), "'", "");
			return rule;
		}

		iter = g_slist_next (iter);
	}
	return NULL;
}

void
rule_free (rulePtr rule)
{
	g_free (rule->value);
	g_free (rule);
}

/* rule conditions */

static gboolean
rule_check_item_title (rulePtr rule, itemPtr item)
{
	return (item->title && g_strstr_len (item->title, -1, rule->value));
}

static gboolean
rule_check_item_description (rulePtr rule, itemPtr item)
{
	return (item->description && g_strstr_len (item->description, -1, rule->value));
}

static gboolean
rule_check_item_all (rulePtr rule, itemPtr item)
{
	return rule_check_item_title (rule, item) || rule_check_item_description (rule, item);
}

static gboolean
rule_check_item_is_unread (rulePtr rule, itemPtr item)
{
	return (0 == item->readStatus);
}

static gboolean
rule_check_item_is_flagged (rulePtr rule, itemPtr item)
{
	return (1 == item->flagStatus);
}

static gboolean
rule_check_item_has_enc (rulePtr rule, itemPtr item)
{
	return item->hasEnclosure;
}

static gboolean
rule_check_item_category (rulePtr rule, itemPtr item)
{
	GSList	*iter = metadata_list_get_values (item->metadata, "category");

	while (iter) {
		if (g_str_equal (rule->value, (gchar *)iter->data))
			return TRUE;

		iter = g_slist_next (iter);
	}

	return FALSE;
}

static gboolean
rule_check_feed_title (rulePtr rule, itemPtr item)
{
	nodePtr feedNode = node_from_id (item->parentNodeId);

	if (!feedNode)
		return FALSE;

	return (feedNode->title && g_strstr_len (feedNode->title, -1, rule->value));
}

static gboolean
rule_check_feed_source (rulePtr rule, itemPtr item)
{
	nodePtr feedNode = node_from_id (item->parentNodeId);
	if (!feedNode)
		return FALSE;

	return (feedNode->subscription && g_strstr_len (feedNode->subscription->source, -1, rule->value));
}

static gboolean
rule_check_parent_folder (rulePtr rule, itemPtr item)
{
	nodePtr node = node_from_id (item->parentNodeId);
	if (!node)
		return FALSE;

	node = node->parent;

	return (node && g_strstr_len (node->title, -1, rule->value));
}

/* rule initialization */

static void
rule_info_add (ruleCheckFunc checkFunc,
          const gchar *ruleId,
          gchar *title,
          gchar *positive,
          gchar *negative,
          gboolean needsParameter)
{
	ruleInfoPtr	ruleInfo;

	ruleInfo = (ruleInfoPtr) g_new0 (struct ruleInfo, 1);
	ruleInfo->ruleId = ruleId;
	ruleInfo->title = title;
	ruleInfo->positive = positive;
	ruleInfo->negative = negative;
	ruleInfo->needsParameter = needsParameter;
	ruleInfo->checkFunc = checkFunc;
	ruleFunctions = g_slist_append (ruleFunctions, ruleInfo);
}

static void
rule_init (void)
{
	debug_enter ("rule_init");

	/*            in-memory check function	feedlist.opml rule id         		  rule menu label       	positive menu option    negative menu option    has param */
	/*            ========================================================================================================================================================================================*/

	rule_info_add (rule_check_item_all,		ITEM_MATCH_RULE_ID,		_("Item"),			_("does contain"),	_("does not contain"),	TRUE);
	rule_info_add (rule_check_item_title,		ITEM_TITLE_MATCH_RULE_ID,	_("Item title"),		_("does contain"),	_("does not contain"),	TRUE);
	rule_info_add (rule_check_item_description,	ITEM_DESC_MATCH_RULE_ID,	_("Item body"),			_("does contain"),	_("does not contain"),	TRUE);
	rule_info_add (rule_check_item_is_unread,	"unread",			_("Read status"),		_("is unread"),		_("is read"),		FALSE);
	rule_info_add (rule_check_item_is_flagged,	"flagged",			_("Flag status"),		_("is flagged"),	_("is unflagged"),	FALSE);
	rule_info_add (rule_check_item_has_enc,		"enclosure",			_("Podcast"),			_("included"),		_("not included"),	FALSE);
	rule_info_add (rule_check_item_category,	"category",			_("Category"),			_("is set"),		_("is not set"),	TRUE);
	rule_info_add (rule_check_feed_title,		FEED_TITLE_MATCH_RULE_ID,	_("Feed title"),		_("does contain"),	_("does not contain"),	TRUE);
	rule_info_add (rule_check_feed_source,		FEED_SOURCE_MATCH_RULE_ID,	_("Feed source"),		_("does contain"),	_("does not contain"),	TRUE);
	rule_info_add (rule_check_parent_folder,	PARENT_FOLDER_MATCH_RULE_ID,	_("Parent folder title"),	_("does contain"),	_("does not contain"),	TRUE);

	debug_exit ("rule_init");
}
