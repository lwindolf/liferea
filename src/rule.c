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
#include "enclosure.h"

#define ITEM_MATCH_RULE_ID		"exact"
#define ITEM_TITLE_MATCH_RULE_ID	"exact_title"
#define ITEM_DESC_MATCH_RULE_ID		"exact_desc"
#define ITEM_AUTHOR_MATCH_RULE_ID	"exact_author"
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
			rule_set_value (rule, value);
			return rule;
		}

		iter = g_slist_next (iter);
	}
	return NULL;
}

void
rule_set_value (rulePtr rule, const gchar *value)
{
	if (rule->value)
		g_free (rule->value);
	if (rule->valueCaseFolded)
		g_free (rule->valueCaseFolded);

	rule->value = common_strreplace (g_strdup (value), "'", "");
	rule->valueCaseFolded = g_utf8_casefold (rule->value, -1);
}

void
rule_free (rulePtr rule)
{
	g_free (rule->value);
	g_free (rule->valueCaseFolded);
	g_free (rule);
}

/* case insensitive strcmp helper function

   To avoid half of the g_utf8_casefold we expect the 2nd value to be already
   case folded!
 */
static const gchar *
rule_strcasecmp (const gchar *a, const gchar *bCaseFold)
{
	gchar		*aCaseFold;
	const gchar	*result;

	aCaseFold = g_utf8_casefold (a, -1);
	result = g_strstr_len (aCaseFold, -1, bCaseFold);
	g_free (aCaseFold);

	return result;
}

/* rule conditions */

static gboolean
rule_check_item_title (rulePtr rule, itemPtr item)
{
	return (item->title && rule_strcasecmp (item->title, rule->valueCaseFolded));
}

static gboolean
rule_check_item_description (rulePtr rule, itemPtr item)
{
	return (item->description && rule_strcasecmp (item->description, rule->valueCaseFolded));
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
rule_check_item_has_podcast (rulePtr rule, itemPtr item)
{
	GSList *iter = metadata_list_get_values (item->metadata, "enclosure");
	gboolean found = FALSE;

	while (iter && !found) {
		enclosurePtr encl = enclosure_from_string ((gchar *)iter->data);
		if (encl != NULL) {
			if (encl->mime && g_str_has_prefix (encl->mime, "audio/")) {
				found = TRUE;
			}
			enclosure_free (encl);
		}
		iter = g_slist_next (iter);
	}
	return found;
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
rule_check_item_author (rulePtr rule, itemPtr item)
{
	GSList	*iter;

	iter = metadata_list_get_values (item->metadata, "author");
	while (iter) {
		if (rule_strcasecmp ((gchar *)iter->data, rule->valueCaseFolded)) {
			return TRUE;
		}
		iter = g_slist_next (iter);
	}

	iter = metadata_list_get_values (item->metadata, "creator");
	while (iter) {
		if (rule_strcasecmp ((gchar *)iter->data, rule->valueCaseFolded)) {
			return TRUE;
		}
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

	return (feedNode->title && rule_strcasecmp (feedNode->title, rule->valueCaseFolded));
}

static gboolean
rule_check_feed_source (rulePtr rule, itemPtr item)
{
	nodePtr feedNode = node_from_id (item->parentNodeId);
	if (!feedNode)
		return FALSE;

	return (feedNode->subscription && rule_strcasecmp (feedNode->subscription->source, rule->valueCaseFolded));
}

static gboolean
rule_check_parent_folder (rulePtr rule, itemPtr item)
{
	nodePtr node = node_from_id (item->parentNodeId);
	if (!node)
		return FALSE;

	node = node->parent;

	return (node && rule_strcasecmp (node->title, rule->valueCaseFolded));
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
	rule_info_add (rule_check_item_author,		ITEM_AUTHOR_MATCH_RULE_ID,	_("Item author"),		_("does contain"),	_("does not contain"),	TRUE);
	rule_info_add (rule_check_item_is_unread,	"unread",			_("Read status"),		_("is unread"),		_("is read"),		FALSE);
	rule_info_add (rule_check_item_is_flagged,	"flagged",			_("Flag status"),		_("is flagged"),	_("is unflagged"),	FALSE);
	rule_info_add (rule_check_item_has_enc,		"enclosure",			_("Enclosure"),			_("included"),		_("not included"),	FALSE);
	rule_info_add (rule_check_item_has_podcast,	"podcast",			_("Podcast"),			_("included"),		_("not included"),	FALSE);
	rule_info_add (rule_check_item_category,	"category",			_("Category"),			_("is set"),		_("is not set"),	TRUE);
	rule_info_add (rule_check_feed_title,		FEED_TITLE_MATCH_RULE_ID,	_("Feed title"),		_("does contain"),	_("does not contain"),	TRUE);
	rule_info_add (rule_check_feed_source,		FEED_SOURCE_MATCH_RULE_ID,	_("Feed source"),		_("does contain"),	_("does not contain"),	TRUE);
	rule_info_add (rule_check_parent_folder,	PARENT_FOLDER_MATCH_RULE_ID,	_("Parent folder title"),	_("does contain"),	_("does not contain"),	TRUE);

	debug_exit ("rule_init");
}
