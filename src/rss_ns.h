/*
   generic namespace handler interface
    
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

#ifndef _NS_H
#define _NS_H

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "feed.h"

// FIXME: remove the following

#include "rss_channel.h"
#include "rss_item.h"

/* -------------------------------------------------------- */
/* interface definitions for RSS namespace handler          */
/* -------------------------------------------------------- */

/* definition of various namespace tag handlers */
typedef void	(*paarseChannelTagFunc)	(RSSChannelPtr cp, xmlNodePtr cur);
typedef void	(*paarseItemTagFunc)	(RSSItemPtr ip, xmlNodePtr cur);

/* handler called during HTML output generation to display
   namespace specific information (e.g. <dc:creator> the 
   handler could return HTML like: "<p>author: Mr. X</a>" */
typedef gchar *	(*outputFunc)	(gpointer obj);

/* struct used to register RDF namespace handler */
typedef struct RSSNsHandler {
	gchar		*prefix;			/**< namespace prefix */

	paarseItemTagFunc	parseItemTag;		/**< item tag parsing method */
	paarseChannelTagFunc	parseChannelTag;	/**< channel tag parsing method */
		
	outputFunc	doItemHeaderOutput;		/**< item header output method */
	outputFunc	doItemFooterOutput;		/**< item footer output method */
	outputFunc	doChannelHeaderOutput;		/**< channel header output method */
	outputFunc	doChannelFooterOutput;		/**< channel footer output method */
} RSSNsHandler;

#endif
