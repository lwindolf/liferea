/**
 * @file rule.c feed/vfolder rule handling
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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
#include "support.h"
#include "feed.h"
#include "item.h"
#include "rule.h"
#include "debug.h"
#include "metadata.h"

#define ITEM_MATCH_RULE_ID		"exact"
#define ITEM_TITLE_MATCH_RULE_ID	"exact_title"
#define ITEM_DESC_MATCH_RULE_ID		"exact_desc"
   
/* rule function interface, each function requires a item
   structure which it matches somehow against its values and
   returns TRUE if the rule was fulfilled and FALSE if not.*/
typedef gboolean (*ruleCheckFuncPtr)	(rulePtr rp, itemPtr ip);
   
/* the rule function list is used to create popup menues in ui_vfolder.c */
struct ruleInfo *ruleFunctions = NULL;
gint nrOfRuleFunctions = 0;

rulePtr rule_new(struct vfolder *vp, const gchar *ruleId, const gchar *value, gboolean additive) {
	ruleInfoPtr	ri;
	rulePtr		rp;
	int		i;
	
	for(i = 0, ri = ruleFunctions; i < nrOfRuleFunctions; i++, ri++) {
		if(0 == strcmp(ri->ruleId, ruleId)) {
			rp = (rulePtr)g_new0(struct rule, 1);
			rp->ruleInfo = ri;
			rp->additive = additive;
			rp->vp = vp;
			
			/* if it is a text matching rule make the text
			   matching value case insensitive */
			if((0 == strcmp(ruleId, ITEM_MATCH_RULE_ID)) ||
			   (0 == strcmp(ruleId, ITEM_TITLE_MATCH_RULE_ID)) ||
			   (0 == strcmp(ruleId, ITEM_DESC_MATCH_RULE_ID))) {
				rp->value = g_utf8_casefold(value, -1);
			} else {
				rp->value = g_strdup(value);
			}
			
			return rp;
		}
	}	
	return NULL;
}

gboolean rule_check_item(rulePtr rp, itemPtr ip) {
	g_assert(ip != NULL);
	
	return (*((ruleCheckFuncPtr)rp->ruleInfo->ruleFunc))(rp, ip);
}

void rule_free(rulePtr rp) {

	g_free(rp->value);
	g_free(rp);
}

/* -------------------------------------------------------------------- */
/* rule checking implementations					*/
/* -------------------------------------------------------------------- */

static gboolean rule_feed_title_match(rulePtr rule, itemPtr item) {
	gboolean	result = FALSE;
	gchar 		*title;
	
	if(NULL != (title = (gchar *)node_get_title(node_from_id(item->nodeId)))) {
		title = g_utf8_casefold(title, -1);
		if(NULL != strstr(title, rule->value))
			result = TRUE;
		g_free(title);
	}
	
	return result;
}

static gboolean rule_item_title_match(rulePtr rule, itemPtr item) {
	gboolean	result = FALSE;
	gchar 		*title;
	
	if(NULL != (title = (gchar *)item_get_title(item))) {
		title = g_utf8_casefold(title, -1);
		if(NULL != strstr(title, rule->value))
			result = TRUE;
		g_free(title);
	}
	
	return result;
}

static gboolean rule_item_description_match(rulePtr rule, itemPtr item) {
	gboolean	result = FALSE;
	gchar 		*desc;

	if(NULL != (desc = (gchar *)item_get_description(item))) {
		desc = g_utf8_casefold(desc, -1);
		if(NULL != strstr(desc, rule->value))
			result = TRUE;
		g_free(desc);
	}
	
	return result;
}

static gboolean rule_item_match(rulePtr rule, itemPtr item) {

	if(rule_item_title_match(rule, item))
		return TRUE;
		
	if(rule_item_description_match(rule, item))
		return TRUE;

	return FALSE;
}

static gboolean rule_item_is_unread(rulePtr rule, itemPtr item) {

	return !(item->readStatus);
}

static gboolean rule_item_is_flagged(rulePtr rule, itemPtr item) {

	return item->flagStatus;
}

static gboolean rule_item_was_updated(rulePtr rule, itemPtr item) {

	return item->updateStatus;
}

static gboolean rule_item_has_enclosure(rulePtr rule, itemPtr item) {

	return item->hasEnclosure;
}

/* rule initialization */

static void rule_add(ruleCheckFuncPtr func, gchar *ruleId, gchar *title, gchar *positive, gchar *negative, gboolean needsParameter) {

	ruleFunctions = (ruleInfoPtr)g_realloc(ruleFunctions, sizeof(struct ruleInfo)*(nrOfRuleFunctions + 1));
	if(NULL == ruleFunctions)
		g_error("could not allocate memory!");
	ruleFunctions[nrOfRuleFunctions].ruleFunc = func;
	ruleFunctions[nrOfRuleFunctions].ruleId = ruleId;
	ruleFunctions[nrOfRuleFunctions].title = title;
	ruleFunctions[nrOfRuleFunctions].positive = positive;
	ruleFunctions[nrOfRuleFunctions].negative = negative;
	ruleFunctions[nrOfRuleFunctions].needsParameter = needsParameter;
	nrOfRuleFunctions++;
}

void rule_init(void) {

	debug_enter("rule_init");

	rule_add(rule_item_match,		ITEM_MATCH_RULE_ID,		_("Item"),		_("does contain"),	_("does not contain"),	TRUE);
	rule_add(rule_item_title_match,		ITEM_TITLE_MATCH_RULE_ID,	_("Item title"),	_("does match"),	_("does not match"),	TRUE);
	rule_add(rule_item_description_match,	ITEM_DESC_MATCH_RULE_ID,	_("Item body"),		_("does match"),	_("does not match"),	TRUE);
	rule_add(rule_feed_title_match,		"feed_title",			_("Feed title"),	_("does match"),	_("does not match"),	TRUE);
	rule_add(rule_item_is_unread,		"unread",			_("Read status"),	_("is unread"),		_("is read"),		FALSE);
	rule_add(rule_item_is_flagged,		"flagged",			_("Flag status"),	_("is flagged"),	_("is unflagged"),	FALSE);
	rule_add(rule_item_was_updated,		"updated",			_("Update status"),	_("was updated"),	_("was not updated"),	FALSE);
	rule_add(rule_item_has_enclosure,	"enclosure",			_("Podcast"),		_("included"),		_("not included"),	FALSE);

	debug_exit("rule_init");
}
