/**
 * @file rule.c feed/vfolder rule handling
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

#include <string.h> /* For strstr() */
#include "support.h"
#include "feed.h"
#include "item.h"
#include "rule.h"
   
/* rule function interface, each function requires a item
   structure which it matches somehow against its values and
   returns TRUE if the rule was fulfilled and FALSE if not.*/
typedef gboolean (*ruleCheckFuncPtr)	(rulePtr rp, itemPtr ip);
   
/* the rule function list is used to create popup menues in ui_vfolder.c */
struct ruleInfo *ruleFunctions = NULL;
gint nrOfRuleFunctions = 0;

rulePtr rule_new(feedPtr fp, const gchar *ruleId, const gchar *value, gboolean additive) {
	ruleInfoPtr	ri;
	rulePtr		rp;
	int		i;
	
	for(i = 0, ri = ruleFunctions; i < nrOfRuleFunctions; i++, ri++) {
		if(0 == strcmp(ri->ruleId, ruleId)) {
			rp = (rulePtr)g_new0(struct rule, 1);
			rp->fp = fp;
			rp->value = g_strdup(value);
			rp->ruleInfo = ri;
			rp->additive = additive;
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

static gboolean rule_exact_title_match(rulePtr rp, itemPtr ip) {

	if(NULL != item_get_title(ip)) {
		if(NULL != strstr(item_get_title(ip), rp->value))
			return TRUE;
	}
	
	return FALSE;
}

static gboolean rule_exact_description_match(rulePtr rp, itemPtr ip) {

	if(NULL != item_get_description(ip)) {
		if(NULL != strstr(item_get_description(ip), rp->value))
			return TRUE;
	}
	
	return FALSE;
}

static gboolean rule_exact_match(rulePtr rp, itemPtr ip) {

	if(rule_exact_title_match(rp, ip))
		return TRUE;
		
	if(rule_exact_description_match(rp, ip))
		return TRUE;

	return FALSE;
}

static gboolean rule_is_unread(rulePtr rp, itemPtr ip) {

	return !(item_get_read_status(ip));
}

static gboolean rule_is_flagged(rulePtr rp, itemPtr ip) {

	return item_get_mark(ip);
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

	rule_add(rule_exact_match,		"exact",		_("Item"),		_("does contain"),	_("does not contain"),	TRUE);
	rule_add(rule_exact_title_match,	"exact_title",		_("Item title"),	_("does match"),	_("does not match"),	TRUE);
	rule_add(rule_exact_description_match,	"exact_desc",		_("Item body"),		_("does match"),	_("does not match"),	TRUE);
	rule_add(rule_is_unread,		"unread",		_("Read status"),	_("is unread"),		_("is read"),		FALSE);
	rule_add(rule_is_flagged,		"flagged",		_("Flag status"),	_("is flagged"),	_("is unflagged"),	FALSE);
}
