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
#ifndef _FILTER_H
#define _FILTER_H

#define	RULE_ADD_EXACT_MATCH	0
#define	RULE_DEL_EXACT_MATCH	1
#define RULE_TYPE_MAX		2

/* structure to store a filter instance */
typedef struct rule {
	gint		type;	/* rule type: e.g. exact match, case insensitive... */
	feedPtr		fp;	/* the feed the filter is applied to */
	gchar		*value;	/* the value of the rule, e.g. a search text */
	/* could be extended... */
} *rulePtr;

/* Checks a new item against all additive rules of all feeds
   except the addition rules of the parent feed. In the second
   step the function checks wether there are parent feed rules,
   which do exclude this item. If there is such a rule the 
   function returns FALSE, otherwise TRUE to signalize if 
   this new item should be added. */
gboolean checkNewItem(itemPtr ip);

/* returns a title to be displayed in the filter editing dialog,
   the returned title must be freed */
gchar * getRuleTitle(rulePtr rp);

#endif
