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
#include "feed.h"
#include "item.h"
#include "filter.h"
   
/* rule function interface, each function requires a item
   structure which it matches somehow against its values and
   returns TRUE if the rule was fulfilled and FALSE if not.*/
typedef gboolean (*ruleCheckFuncPtr)	(rulePtr rp, itemPtr ip);

typedef struct ruleInfo {
	ruleCheckFuncPtr	ruleFunc;	/* the rules test function */
	gchar			*ruleId;	/* rule id for cache file storage */
	gchar			*title;		/* rule type title for dialogs */
	gboolean		additive;	/* is it a removing or adding rule */
} *ruleInfoPtr;

/* check function prototypes */
static gboolean rule_exact_match(rulePtr rp, itemPtr ip);

/* Definition of all implemented filters, this tables entry positions
   _MUST_ match the RULE_* definitions! */
static struct ruleInfo ruleFunctions[] = {
	{ rule_exact_match,	"add_exact",	"add items matching \"%s\"",	TRUE },
	{ rule_exact_match,	"del_exact",	"hide items matching \"%s\"",	FALSE },
	{ NULL,			NULL,		NULL,				FALSE }
};

rulePtr rule_new(feedPtr fp, gchar *ruleId, gchar *value) {
	ruleInfoPtr	ri;
	rulePtr		rp;
	
	ri = ruleFunctions;
	while(NULL != ri->ruleFunc) {
		if(0 == strcmp(ri->ruleId, ruleId)) {
			rp = (rulePtr)g_new0(rulePtr, 1);
			rp->fp = fp;
			rp->value = g_strdup(value);
			rp->ruleInfo = ri;
			return rp;
		}
		ri++;
	}	
	return NULL;
}

gboolean rule_is_additive(rulePtr rp) {

	return ((ruleInfoPtr)(rp->ruleInfo))->additive;
}

gboolean rule_check_item(rulePtr rp, itemPtr ip) {
	ruleInfoPtr	ruleInfo;
	gboolean	matches = FALSE;

	ruleInfo = (struct ruleInfo *)rp->ruleInfo;
	return (*(ruleInfo->ruleFunc))(rp, ip);
}

/* returns a title to be displayed in the filter editing dialog,
   the returned title must be freed */
gchar * rule_get_title(rulePtr rp) {
	ruleInfoPtr	ruleInfo;
	
	ruleInfo = ruleFunctions;
	while(NULL != ruleInfo->ruleFunc) {
		if(rp->ruleInfo == ruleInfo)
			return ruleInfo->title;
		ruleInfo++;
	}	
	return NULL;
}

/* -------------------------------------------------------------------- */
/* rule checking implementations					*/
/* -------------------------------------------------------------------- */

static gboolean rule_exact_match(rulePtr rp, itemPtr ip) {

	g_assert(rp != NULL);
	g_assert(ip != NULL);

	if(NULL != item_get_title(ip)) {
		if(NULL != strstr(item_get_title(ip), rp->value))
			return TRUE;
	}
	
	if(NULL != item_get_description(ip)) {
		if(NULL != strstr(item_get_description(ip), rp->value))
			return TRUE;
	}

	return FALSE;
}
