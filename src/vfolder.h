/*
   VFolder functionality
      
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

#ifndef _VFOLDER_H
#define _VFOLDER_H

#include "feed.h"
#include "item.h"

/* structure to store search rules of a VFolder */
typedef struct rule {
	//gint		type;	/* rule type: e.g. exact match, case insensitive... */
	gchar		*value;	/* the value of the rule, e.g. a search text */
	//rulePtr	next;	/* next rule */
} *rulePtr;

/* standard feed/item type interface */
feedHandlerPtr	initVFolderFeedHandler(void);

/* VFolder interface */
void		initVFolders(void);
void		removeOldItemsFromVFolder(feedPtr vp, feedPtr fp);
void 		removeOldItemsFromVFolders(gpointer key, gpointer value, gpointer userdata);	// FIXME!
void 		scanFeed(gpointer key, gpointer value, gpointer userdata);		// FIXME!
void		addItemToVFolder(feedPtr vp, feedPtr fp, itemPtr ip);
void		setVFolderRules(feedPtr vp, rulePtr rp);
rulePtr		getVFolderRules(feedPtr vp);
gboolean	matchVFolderRules(feedPtr vp, gchar *string);

#endif
