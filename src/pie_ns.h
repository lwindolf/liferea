/*
   PIE namespace handler interface
    
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef _PIE_NS_H
#define _PIE_NS_H

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "pie_feed.h"
#include "pie_entry.h"

/* -------------------------------------------------------- */
/* interface definitions for PIE namespace handler          */
/* -------------------------------------------------------- */

/* definition of various namespace tag handlers */
typedef void	(*parseFeedTagFunc)	(PIEFeedPtr cp, xmlDocPtr doc, xmlNodePtr cur);
typedef void	(*parseEntryTagFunc)	(PIEEntryPtr ip, xmlDocPtr doc, xmlNodePtr cur);

/* handler called during HTML output generation to display
   namespace specific information (e.g. <dc:creator> the 
   handler could return HTML like: "<p>author: Mr. X</a>" */
typedef gchar *	(*PIEOutputFunc)	(gpointer obj);

/* struct used to register RDF namespace handler */
typedef struct PIENsHandler {
	parseEntryTagFunc	parseItemTag;
	parseFeedTagFunc	parseChannelTag;
		
	PIEOutputFunc	doItemHeaderOutput;
	PIEOutputFunc	doItemFooterOutput;	
	PIEOutputFunc	doChannelHeaderOutput;
	PIEOutputFunc	doChannelFooterOutput;	
} PIENsHandler;

#endif
