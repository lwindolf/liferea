/*
   feed/vfolder filter handling
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h> /* For strstr() */
#include "feed.h"
#include "item.h"
#include "filter.h"
#include "vfolder.h"

/* The following code implements the filtering and vfolder functionality.
   It allows each feed to define a set of filters, which can either
   remove item from the feed or even add items from other feed. The last
   possibility allows to define vfolders using empty feed structures. */
   
/* filter check function interface, the function require a item
   structure which it matches somehow against its values and
   returns TRUE if the rule was fulfilled and FALSE if not.*/
typedef gboolean (*ruleCheckFunc)	(rulePtr rp, itemPtr ip);

struct ruleCheckFunctionInfo {
	ruleCheckFunc	ruleFunc;	/* the rules test function */
	gboolean	additive;	/* is it a removing or adding rule */
};

/* check function prototypes */
gboolean rule_exact_match(rulePtr rp, itemPtr ip);

/* Definition of all implemented filters, this tables entry positions
   _MUST_ match the RULE_* definitions! */
struct ruleCheckFunctionInfo ruleFunctions[] = {
	{ rule_exact_match,	TRUE },	/* RULE_ADD_EXACT_MATCH */
	{ rule_exact_match,	FALSE }	/* RULE_DEL_EXACT_MATCH */
};

/* rule type titles for the filter editing dialog */
gchar * ruleTypeTitles[] = {
	"add items matching \"%s\"",
	"hide items matching \"%s\""
};

/* list of the rules of all feeds, use to check new items against these rules */
GSList	*allRules;

/* -------------------------------------------------------------------- */
/* rule checking functionality						*/
/* -------------------------------------------------------------------- */

/* Checks a new item against all additive rules of all feeds
   except the addition rules of the parent feed. In the second
   step the function checks wether there are parent feed rules,
   which do exclude this item. If there is such a rule the 
   function returns FALSE, otherwise TRUE to signalize if 
   this new item should be added. */
gboolean checkNewItem(itemPtr ip) {
	GSList		*rule;
	ruleCheckFunc	rf;
	rulePtr		r;
	
	/* important: we assume the parent pointer of the item is set! */
	g_assert(NULL != ip->fp);
	
	/* check against all additive rules */
	rule = allRules;
/*	while(NULL != rule) {
		r = rule->data;
		rf = ruleFunctions[r->type].ruleFunc;
		g_assert(NULL == rf);
		g_assert(RULE_TYPE_MAX > r->type);
		
		if(TRUE == ruleFunctions[r->type].additive)
			if(TRUE == (*rf)(r, ip)) {
				g_assert(FST_VFOLDER == r->fp->type);
				addItemToVFolder(r->fp, ip);
			}
			
		rule = g_slist_next(rule);
	}*/
	
	/* check against non additive rules of parent feed */
/*	rule = ((feedPtr)(ip->fp))->filter;
	while(NULL != rule) {
		r = rule->data;
		rf = ruleFunctions[r->type].ruleFunc;
		g_assert(NULL == rf);
		g_assert(RULE_TYPE_MAX > r->type);

		if(FALSE == ruleFunctions[r->type].additive)
			if(FALSE == (*rf)(r, ip))
				return FALSE;
					
		rule = g_slist_next(rule);
	}*/
	return TRUE;
}

/* returns a title to be displayed in the filter editing dialog,
   the returned title must be freed */
gchar * getRuleTitle(rulePtr rp) {

	g_assert(NULL != rp);
	g_assert(RULE_TYPE_MAX > rp->type);
	
	return g_strdup_printf(ruleTypeTitles[rp->type], rp->value);
}

/* -------------------------------------------------------------------- */
/* rule checking implementations					*/
/* -------------------------------------------------------------------- */

gboolean rule_exact_match(rulePtr rp, itemPtr ip) {

	g_assert(rp != NULL);
	g_assert(ip != NULL);

	if(NULL != item_get_title(ip)) {
		if(NULL != strstr(item_get_title(ip), rp->value)) {
			return TRUE;
		}
	}
	
	if(NULL != item_get_description(ip)) {
		if(NULL != strstr(item_get_description(ip), rp->value)) {
			return TRUE;
		}
	}

	return FALSE;
}
