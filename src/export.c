/**
 * @file export.c OPML feedlist import&export
 *
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <libxml/tree.h>
#include <string.h>
#include "feed.h"
#include "vfolder.h"
#include "rule.h"
#include "conf.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "favicon.h"
#include "debug.h"
#include "plugin.h"
#include "fl_providers/fl_plugin.h"
#include "ui/ui_node.h"
#include "ui/ui_feedlist.h"

struct exportData {
	gboolean internal; /**< Include all the extra Liferea-specific tags */
	xmlNodePtr cur;
};

/* Used for exporting, this adds a folder or feed's node to the XML tree */
static void append_node_tag(nodePtr np, gpointer userdata) {
	xmlNodePtr 	cur = ((struct exportData*)userdata)->cur;
	gboolean	internal = ((struct exportData*)userdata)->internal;
	xmlNodePtr	childNode, ruleNode;
	struct exportData data;
	GSList		*iter;
	vfolderPtr	vp;
	feedPtr		fp;
	rulePtr		rule;
	
	debug_enter("append_node_tag");

	childNode = xmlNewChild(cur, NULL, BAD_CAST"outline", NULL);

	xmlNewProp(childNode, BAD_CAST"title", BAD_CAST node_get_title(np));
	xmlNewProp(childNode, BAD_CAST"text", BAD_CAST node_get_title(np)); /* The OPML spec requires "text" */
	
	/* add node type specific stuff */
	switch(np->type) {
	   case FST_FOLDER:
		/* add folder children */
		if(internal) {
			if(ui_node_is_folder_expanded(np))
				xmlNewProp(childNode, BAD_CAST"expanded", NULL);
			else
				xmlNewProp(childNode, BAD_CAST"collapsed", NULL);
		}
		debug1(DEBUG_CACHE, "adding folder %s...", node_get_title(np));
		data.cur = childNode;
		data.internal = internal;
		ui_feedlist_do_for_all_data(np, ACTION_FILTER_CHILDREN, append_node_tag, (gpointer)&data);
		break;
	   case FST_FEED:
		/* add feed properties */
		fp = (feedPtr)np->data;
		const gchar *type = feed_type_fhp_to_str(feed_get_fhp(fp));
		gchar *interval = g_strdup_printf("%d",feed_get_update_interval(fp));
		gchar *cacheLimit = NULL;

		xmlNewProp(childNode, BAD_CAST"description", BAD_CAST feed_get_title(fp));
		xmlNewProp(childNode, BAD_CAST"type", BAD_CAST type);
		if(feed_get_html_url(fp) != NULL)
			xmlNewProp(childNode, BAD_CAST"htmlUrl", BAD_CAST feed_get_html_url(fp));
		else
			xmlNewProp(childNode, BAD_CAST"htmlUrl", BAD_CAST "");
		xmlNewProp(childNode, BAD_CAST"xmlUrl", BAD_CAST feed_get_source(fp));
		xmlNewProp(childNode, BAD_CAST"updateInterval", BAD_CAST interval);

		if(fp->cacheLimit >= 0)
			cacheLimit = g_strdup_printf("%d", fp->cacheLimit);
		if(fp->cacheLimit == CACHE_UNLIMITED)
			cacheLimit = g_strdup("unlimited");
		if(cacheLimit != NULL)
			xmlNewProp(childNode, BAD_CAST"cacheLimit", BAD_CAST cacheLimit);

		if(feed_get_filter(fp) != NULL)
			xmlNewProp(childNode, BAD_CAST"filtercmd", BAD_CAST feed_get_filter(fp));

		if(internal) {
			if(fp->noIncremental)
				xmlNewProp(childNode, BAD_CAST"noIncremental", BAD_CAST"true");
				
			xmlNewProp(childNode, BAD_CAST"id", BAD_CAST node_get_id(np));
			if(fp->lastPoll.tv_sec > 0) {
				gchar *lastPoll = g_strdup_printf("%ld", fp->lastPoll.tv_sec);
				xmlNewProp(childNode, BAD_CAST"lastPollTime", BAD_CAST lastPoll);
				g_free(lastPoll);
			}
			if(fp->lastFaviconPoll.tv_sec > 0) {
				gchar *lastPoll = g_strdup_printf("%ld", fp->lastFaviconPoll.tv_sec);
				xmlNewProp(childNode, BAD_CAST"lastFaviconPollTime", BAD_CAST lastPoll);
				g_free(lastPoll);
			}
			if(TRUE == fp->encAutoDownload)
				xmlNewProp(childNode, BAD_CAST"encAutoDownload", BAD_CAST"true");
		}
		debug6(DEBUG_CACHE, "adding feed: title=%s type=%s source=%d id=%s interval=%s cacheLimit=%s", feed_get_title(fp), type, feed_get_source(fp), node_get_id(np), interval, cacheLimit);
		g_free(cacheLimit);
		g_free(interval);
		break;
	    case FST_VFOLDER:		
		xmlNewProp(childNode, BAD_CAST"type", BAD_CAST "vfolder");
		/* add vfolder rules */
		vp = (vfolderPtr)np->data;
		iter = vp->rules;
		while(NULL != iter) {
			rule = iter->data;
			ruleNode = xmlNewChild(childNode, NULL, BAD_CAST"outline", NULL);
			xmlNewProp(ruleNode, BAD_CAST"type", BAD_CAST "rule");
			xmlNewProp(ruleNode, BAD_CAST"text", BAD_CAST rule->ruleInfo->title);
			xmlNewProp(ruleNode, BAD_CAST"rule", BAD_CAST rule->ruleInfo->ruleId);
			xmlNewProp(ruleNode, BAD_CAST"value", BAD_CAST rule->value);
			if(TRUE == rule->additive)
				xmlNewProp(ruleNode, BAD_CAST"additive", BAD_CAST "true");
			else
				xmlNewProp(ruleNode, BAD_CAST"additive", BAD_CAST "false");

			iter = g_slist_next(iter);
		}
		break;
	    default:
		g_warning("fatal: unknown node type %d when exporting!", np->type);
		break;
	}

	/* export general node properties */
	if(internal) {
		if(np->sortColumn == IS_LABEL)
			xmlNewProp(childNode, BAD_CAST"sortColumn", BAD_CAST"title");
		else if(np->sortColumn == IS_TIME)
			xmlNewProp(childNode, BAD_CAST"sortColumn", BAD_CAST"time");
		if(FALSE == np->sortReversed)
			xmlNewProp(childNode, BAD_CAST"sortReversed", BAD_CAST"false");
			
		if(TRUE == node_get_two_pane_mode(np))
			xmlNewProp(childNode, BAD_CAST"twoPane", BAD_CAST"true");
	}
	
	debug_exit("append_node_tag");
}


int export_OPML_feedlist(const gchar *filename, gboolean internal) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur, opmlNode;
	gint		error = 0;
	int		old_umask;

	debug_enter("export_OPML_feedlist");
	
	if(NULL != (doc = xmlNewDoc("1.0"))) {	
		if(NULL != (opmlNode = xmlNewDocNode(doc, NULL, BAD_CAST"opml", NULL))) {
			xmlNewProp(opmlNode, BAD_CAST"version", BAD_CAST"1.0");
			
			/* create head */
			if(NULL != (cur = xmlNewChild(opmlNode, NULL, BAD_CAST"head", NULL))) {
				xmlNewTextChild(cur, NULL, BAD_CAST"title", BAD_CAST"Liferea Feed List Export");
			}
			
			/* create body with feed list */
			if(NULL != (cur = xmlNewChild(opmlNode, NULL, BAD_CAST"body", NULL))) {
				struct exportData data;
				data.internal = internal;
				data.cur = cur;
				ui_feedlist_do_for_all_data(NULL, ACTION_FILTER_CHILDREN, append_node_tag, (gpointer)&data);
			}
			
			xmlDocSetRootElement(doc, opmlNode);		
		} else {
			g_warning("could not create XML feed node for feed cache document!");
			error = 1;
		}
		
		if(internal)
			old_umask = umask(077);
			
		if(-1 == xmlSaveFormatFileEnc(filename, doc, NULL, 1)) {
			g_warning("Could not export to OPML file!!");
			error = 1;
		}
		
		if(internal)
			umask(old_umask);
			
		xmlFreeDoc(doc);
	} else {
		g_warning("could not create XML document!");
		error = 1;
	}
	
	debug_exit("export_OPML_feedlist");
	return error;
}

static int parse_integer(gchar *str, int def) {
	int num;
	if (str == NULL)
		return def;
	if (0==(sscanf(str,"%d",&num)))
		num = def;
	
	return num;
}

static long parse_long(gchar *str, long def) {
	long num;

	if(str == NULL)
		return def;
	if(0 == (sscanf(str,"%ld",&num)))
		num = def;
	
	return num;
}

/** 
 * called by import_parse_outline to parse all children outline tags
 * as vfolder rule descriptions 
 */
static void import_parse_children_as_rules(xmlNodePtr cur, vfolderPtr vp) {
	xmlChar		*type, *ruleId, *value, *additive;
	
	/* process any children */
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if(!xmlStrcmp(cur->name, BAD_CAST"outline")) {
			type = xmlGetProp(cur, BAD_CAST"type");
			if(type != NULL && !xmlStrcmp(type, BAD_CAST"rule")) {

				ruleId = xmlGetProp(cur, BAD_CAST"rule");
				value = xmlGetProp(cur, BAD_CAST"value");
				additive = xmlGetProp(cur, BAD_CAST"additive");

				if((NULL != ruleId) && (NULL != value)) {			
					debug2(DEBUG_CACHE, "loading rule \"%s\" \"%s\"\n", ruleId, value);

					if(additive != NULL && !xmlStrcmp(additive, BAD_CAST"true"))
						vfolder_add_rule(vp, ruleId, value, TRUE);
					else
						vfolder_add_rule(vp, ruleId, value, FALSE);
				} else {
					g_warning("ignoring invalid rule entry in feed list...\n");
				}
				
				xmlFree(ruleId);
				xmlFree(value);
				xmlFree(additive);
			}
			xmlFree(type);
		}
		cur = cur->next;
	}
}

static void import_parse_outline(xmlNodePtr cur, nodePtr parentNode, flNodeHandler *handler, gboolean trusted) {
	gchar		*cacheLimitStr, *filter, *intervalStr, *lastPollStr, *htmlUrlStr, *sortStr;
	gchar		*title, *source, *typeStr, *tmp;
	nodePtr		np = NULL;
	feedPtr		fp = NULL;
	vfolderPtr	vp = NULL;
	gboolean	dontParseChildren = FALSE;
	gint		interval;
	gchar		*id = NULL;
	
	debug_enter("import_parse_outline");
	
	/* process the outline node */	
	np = node_new();
	np->handler = handler;

	title = xmlGetProp(cur, BAD_CAST"title");
	if(title == NULL || !xmlStrcmp(title, BAD_CAST"")) {
		if(title != NULL)
			xmlFree(title);
		title = xmlGetProp(cur, BAD_CAST"text");
	}
	node_set_title(np, title);
	
	if(NULL == (source = xmlGetProp(cur, BAD_CAST"xmlUrl")))
		source = xmlGetProp(cur, BAD_CAST"xmlurl");	/* e.g. for AmphetaDesk */
	
	if(NULL != source) { /* Reading a feed */
	
		filter = xmlGetProp(cur, BAD_CAST"filtercmd");

		if(!trusted && filter != NULL) {
			/* FIXME: Display warning dialog asking if the command
			   is safe? */
			tmp = g_strdup_printf("unsafe command: %s", filter);
			g_free(filter);
			filter = tmp;
		}
		
		if(!trusted && source[0] == '|') {
			/* FIXME: Display warning dialog asking if the command
			   is safe? */
			tmp = g_strdup_printf("unsafe command: %s", source);
			g_free(source);
			source = tmp;
		}
		
		intervalStr = xmlGetProp(cur, BAD_CAST"updateInterval");
		interval = parse_integer(intervalStr, -1);
		xmlFree(intervalStr);

		/* The id should only be used from feedlist.opml. Otherwise,
		   it could cause corruption if the same id was imported
		   multiple times. */
		if(trusted)
			id = xmlGetProp(cur, BAD_CAST"id");

		/* get type attribute and use it to assign a value to
		   fhp. fhp will default to NULL. */
		typeStr = xmlGetProp(cur, BAD_CAST"type");
		if((NULL != typeStr) && (0 == strcmp("vfolder", typeStr))) {
			dontParseChildren = TRUE;
			vp = vfolder_new(np);
			import_parse_children_as_rules(cur, vp);
			node_add_data(np, FST_VFOLDER, (gpointer)vp); // FIXME: make node adding generic

			debug1(DEBUG_CACHE, "import vfolder: title=%s", title);
		} else if((NULL != typeStr) && (0 == strcmp("plugin", typeStr))) {
			// FIXME:
			debug0(DEBUG_CACHE, "import plugin");
			g_warning("implement me");
		} else {
			fp = feed_new();
			node_add_data(np, FST_FEED, (gpointer)fp); // FIXME: make node adding generic
			fp->fhp = feed_type_str_to_fhp(typeStr);

			/* Set the feed cache limit */
			cacheLimitStr = xmlGetProp(cur, BAD_CAST"cacheLimit");
			if(cacheLimitStr != NULL && !xmlStrcmp(cacheLimitStr, "unlimited")) {
				fp->cacheLimit = CACHE_UNLIMITED;
			} else
				fp->cacheLimit = parse_integer(cacheLimitStr, CACHE_DEFAULT);
			xmlFree(cacheLimitStr);
		
			/* Obtain the htmlUrl */
			htmlUrlStr = xmlGetProp(cur, BAD_CAST"htmlUrl");
			if(htmlUrlStr != NULL && 0 != xmlStrcmp(htmlUrlStr, ""))
				feed_set_html_url(fp, htmlUrlStr);
			xmlFree(htmlUrlStr);
		
			tmp = xmlGetProp(cur, BAD_CAST"noIncremental");
			if(NULL != tmp && !xmlStrcmp(tmp, BAD_CAST"true"))
				fp->noIncremental = TRUE;
			xmlFree(tmp);
		
			/* Last poll time*/
			lastPollStr = xmlGetProp(cur, BAD_CAST"lastPollTime");
			fp->lastPoll.tv_sec = parse_long(lastPollStr, 0L);
			fp->lastPoll.tv_usec = 0L;
			if(lastPollStr != NULL)
				xmlFree(lastPollStr);
		
			lastPollStr = xmlGetProp(cur, BAD_CAST"lastFaviconPollTime");
			fp->lastFaviconPoll.tv_sec = parse_long(lastPollStr, 0L);
			fp->lastFaviconPoll.tv_usec = 0L;
			if(lastPollStr != NULL)
				xmlFree(lastPollStr);

			tmp = xmlGetProp(cur, BAD_CAST"encAutoDownload");
			if(NULL != tmp && !xmlStrcmp(tmp, BAD_CAST"true"))
				fp->encAutoDownload = TRUE;
			if(tmp != NULL)
				xmlFree(tmp);

			/* set feed properties available from the OPML feed list 
			   they may be overwritten by the values of the cache file
			   but we need them in case the cache file loading fails */
		
			feed_set_source(fp, source);
			feed_set_filter(fp, filter);
			feed_set_title(fp, title);
			feed_set_update_interval(fp, interval);

			debug6(DEBUG_CACHE, "import feed: title=%s source=%s typeStr=%s id=%s interval=%d lastpoll=%ld", title, source, typeStr, id, interval, fp->lastPoll.tv_sec);
		}

		/* sorting order */
		sortStr = xmlGetProp(cur, BAD_CAST"sortColumn");
		if(sortStr != NULL) {
			if(!xmlStrcmp(sortStr, "title"))
				np->sortColumn = IS_LABEL;
			else if(!xmlStrcmp(sortStr, "time"))
				np->sortColumn = IS_TIME;
			xmlFree(sortStr);
		}
		sortStr = xmlGetProp(cur, BAD_CAST"sortReversed");
		if(sortStr != NULL && !xmlStrcmp(sortStr, BAD_CAST"false"))
			np->sortReversed = FALSE;
		if(sortStr != NULL)
			xmlFree(sortStr);
		
		tmp = xmlGetProp(cur, BAD_CAST"twoPane");
		if(NULL != tmp && !xmlStrcmp(tmp, BAD_CAST"true"))
			node_set_two_pane_mode(np, TRUE);
		if(tmp != NULL)
			xmlFree(tmp);

		
		if(id != NULL) {
			node_set_id(np, id);
			xmlFree(id);
			/* don't load here, because it's not sure that all vfolders are loaded */
		} else {
			id = node_new_id();
			node_set_id(np, id);
			debug1(DEBUG_CACHE, "seems to be an import, setting new id: %s and doing first download...", id);
			g_free(id);			
			node_request_update(np, (xmlHasProp(cur, BAD_CAST"updateInterval") ? 0 : FEED_REQ_RESET_UPDATE_INT)
				                | FEED_REQ_DOWNLOAD_FAVICON
				                | FEED_REQ_AUTH_DIALOG);
		}

		g_print("add np=%d, title=%s\n", np, title);
		feedlist_add_node(parentNode, np, -1);
		
		if(source != NULL)
			xmlFree(source);
		if(filter != NULL)
			xmlFree(filter);
		xmlFree(typeStr);
		
	} else { /* It is a folder */
		debug1(DEBUG_CACHE, "adding folder \"%s\"", title);
		node_add_data(np, FST_FOLDER, NULL);	// FIXME: make node adding generic
		feedlist_add_node(parentNode, np, -1);

		if(NULL != xmlHasProp(cur, BAD_CAST"expanded"))
			ui_node_set_expansion(np, TRUE);
		if(NULL != xmlHasProp(cur, BAD_CAST"collapsed"))
			ui_node_set_expansion(np, FALSE);
	}

	if(title != NULL)
		xmlFree(title);

	if(!dontParseChildren) {
		/* process any children */
		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
				import_parse_outline(cur, np, handler, trusted);
			cur = cur->next;				
		}
	}
	
	debug_exit("import_parse_outline");
}

static void import_parse_body(xmlNodePtr n, nodePtr parentNode, flNodeHandler *handler, gboolean trusted) {
	xmlNodePtr cur;
	
	cur = n->xmlChildrenNode;
	while(cur != NULL) {
		if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
			import_parse_outline(cur, parentNode, handler, trusted);
		cur = cur->next;
	}
}

static void import_parse_OPML(xmlNodePtr n, nodePtr parentNode, flNodeHandler *handler, gboolean trusted) {
	xmlNodePtr cur;
	
	cur = n->xmlChildrenNode;
	while(cur != NULL) {
		/* we ignore the head */
		if((!xmlStrcmp(cur->name, BAD_CAST"body"))) {
			import_parse_body(cur, parentNode, handler, trusted);
		}
		cur = cur->next;
	}	
}

void import_OPML_feedlist(const gchar *filename, nodePtr parentNode, flNodeHandler *handler, gboolean showErrors, gboolean trusted) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	
	debug1(DEBUG_CACHE, "Importing OPML file: %s", filename);
	
	/* read the feed list */
	if(NULL == (doc = xmlParseFile(filename))) {
		if(showErrors)
			ui_show_error_box(_("XML error while reading cache file! Could not import \"%s\"!"), filename);
		else
			g_warning(_("XML error while reading cache file! Could not import \"%s\"!"), filename);
	} else {
		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			if(showErrors)
				ui_show_error_box(_("Empty document! OPML document \"%s\" should not be empty when importing."), filename);
			else
				g_warning(_("Empty document! OPML document \"%s\" should not be empty when importing."), filename);
		} else {
			while(cur != NULL) {
				if(!xmlIsBlankNode(cur)) {
					if(!xmlStrcmp(cur->name, BAD_CAST"opml")) {
						import_parse_OPML(cur, parentNode, handler, trusted);
					} else {
						if(showErrors)
							ui_show_error_box(_("\"%s\" is not a valid OPML document! Liferea cannot import this file!"), filename);
						else
							g_warning(_("\"%s\" is not a valid OPML document! Liferea cannot import this file!"), filename);
					}
				}
				cur = cur->next;
			}
		}
		xmlFreeDoc(doc);
	}
}


/* UI stuff */

void on_import_activate_cb(const gchar *filename, gpointer user_data) {
	nodePtr		np;
	
	if(filename != NULL) {
		np = node_new();
		node_set_title(np, _("Imported feed list"));
		node_add_data(np, FST_FOLDER, NULL);
		
		/* add the new folder to the model */
		feedlist_add_node(NULL, np, -1);
		
		import_OPML_feedlist(filename, np, np->handler, TRUE, FALSE);
	}
}

void on_import_activate(GtkMenuItem *menuitem, gpointer user_data) {

	ui_choose_file(_("Import Feed List"), GTK_WINDOW(mainwindow), _("Import"), FALSE, on_import_activate_cb, NULL, NULL, NULL);
}

static void on_export_activate_cb(const gchar *filename, gpointer user_data) {
	gint error = 0;

	if (filename != NULL) {
		error = export_OPML_feedlist(filename, FALSE);
	
		if(0 != error)
			ui_show_error_box(_("Error while exporting feed list!"));
		else 
			ui_show_info_box(_("Feed List exported!"));
	}
}


void on_export_activate(GtkMenuItem *menuitem, gpointer user_data) {
	
	ui_choose_file(_("Export Feed List"), GTK_WINDOW(mainwindow), _("Export"), TRUE, on_export_activate_cb, "feedlist.opml", NULL, NULL);
}

