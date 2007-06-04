/**
 * @file bloglines_source.c Bloglines feed list source support
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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

#include <glib.h>
#include <libxml/xpath.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <unistd.h>

#include "support.h"
#include "conf.h"
#include "debug.h"
#include "node.h"
#include "fl_sources/bloglines_source-ui.h"
#include "fl_sources/bloglines_source-cb.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"

static void bloglines_source_auto_update(nodePtr node, GTimeVal *now) {

	if(node->source->updateState->lastPoll.tv_sec + getNumericConfValue(DEFAULT_UPDATE_INTERVAL)*60 <= now->tv_sec)
		opml_source_update(node, now);	
}

static void bloglines_source_init(void) { }

static void bloglines_source_deinit(void) { }

/* node source type definition */

static struct nodeSourceType nst = {
	NODE_SOURCE_TYPE_API_VERSION,
	"fl_bloglines",
	NULL,	/* name defined below */
	NULL,	/* description defined below */
	NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION,
	bloglines_source_init,
	bloglines_source_deinit,
	ui_bloglines_source_get_account_info,
	opml_source_remove,
	opml_source_import,
	opml_source_export,
	opml_source_get_feedlist,
	opml_source_update,
	bloglines_source_auto_update
};

nodeSourceTypePtr bloglines_source_get_type(void) { 

	if(!nst.name)
		nst.name = _("Bloglines");
	if(!nst.description)
		nst.description = _("Integrate the feed list of your Bloglines account. Liferea will "
	                            "present your Bloglines subscription as a read-only subtree in the feed list.");

	return &nst;
}
