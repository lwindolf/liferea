/*
   OCS namespace handler interface
    
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

#ifndef _OCS_NS_H
#define _OCS_NS_H

#include "ocs_dir.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

/* -------------------------------------------------------- */
/* interface definitions for OCS namespace handler          */
/* -------------------------------------------------------- */

/* definition of various namespace tag handlers */
typedef void	(*parseOCSTagFunc)	(gpointer p, xmlDocPtr doc, xmlNodePtr cur);

/* struct used to register RDF namespace handler */
typedef struct OCSNsHandler {
	parseOCSTagFunc	parseDirectoryTag;
	parseOCSTagFunc	parseDirEntryTag;
	parseOCSTagFunc	parseFormatTag;		
} OCSNsHandler;

#endif
