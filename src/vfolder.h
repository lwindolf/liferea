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

#include "backend.h"

/* structure to store search rules of a VFolder */
typedef struct rule {
	//gint		type;	/* rule type: e.g. exact match, case insensitive... */
	gchar		*value;	/* the value of the rule, e.g. a search text */
	//rulePtr	next;	/* next rule */
} *rulePtr;

typedef struct VFolder {
	/* type, key and keyprefix HAVE TO BE THE FIRST elements of 
	   this structure, order is important! */
	gint		type;		/* FST_VFOLDER for RSS channels */
	gchar		*key;		/* configuration storage key */	
	gchar		*keyprefix;

	GSList		*items;		/* the item list */
	GHashTable	*itemtypes;	/* hashtable to lookup itemtypes */
	gchar		*usertitle;	/* vfolder title */
	
	rulePtr		rules;		/* the search rules of this VFolder */
	// FIXME: still to complicated to support...
	//gint		unreadCounter;	/* counter of unread items */
} *VFolderPtr;

/* standard feed/item type interface */
feedHandlerPtr	initVFolderFeedHandler(void);
itemHandlerPtr	initVFolderItemHandler(void);

/* VFolder interface */
void		removeOldItemsFromVFolder(VFolderPtr vp, gpointer ep);
void		addItemToVFolder(VFolderPtr vp, gpointer ep, gpointer ip, gint type);
void		setVFolderRules(VFolderPtr vp, rulePtr rp);
rulePtr		getVFolderRules(VFolderPtr vp);
gboolean	matchVFolderRules(VFolderPtr vp, gchar *string);

#endif
