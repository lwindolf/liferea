/**
 * @file export.c OPML feedlist import&export
 *
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <libxml/tree.h>
#include "feed.h"
#include "folder.h"
#include "vfolder.h"
#include "rule.h"
#include "conf.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "favicon.h"
#include "ui_folder.h"
#include "ui_feedlist.h"
#include "debug.h"

/* _() for HELP1URL to allow localised help feeds */
#define HELP1URL_1 	_("http://liferea.sf.net/help/help")
#define HELP1URL_2	_(".rdf")

struct exportData {
	gboolean internal; /**< Include all the extra Liferea-specific tags */
	xmlNodePtr cur;
};

/* Used for exporting, this adds a folder or feed's node to the XML tree */
static void append_node_tag(nodePtr ptr, gpointer userdata) {
	xmlNodePtr 	cur = ((struct exportData*)userdata)->cur;
	gboolean	internal = ((struct exportData*)userdata)->internal;
	xmlNodePtr	childNode, ruleNode;
	GSList		*iter;
	rulePtr		rule;
	
	debug_enter("append_node_tag");
	
	if(FST_FOLDER == ptr->type) {
		folderPtr folder = (folderPtr)ptr;
		struct exportData data;
		childNode = xmlNewChild(cur, NULL, BAD_CAST"outline", NULL);
		xmlNewProp(childNode, BAD_CAST"title", BAD_CAST folder_get_title(folder));
		
		if(internal) {
			if(ui_is_folder_expanded(folder))
				xmlNewProp(childNode, BAD_CAST"expanded", NULL);
			else
				xmlNewProp(childNode, BAD_CAST"collapsed", NULL);
		}
		debug1(DEBUG_CONF, "adding folder %s...", folder_get_title(folder));
		data.cur = childNode;
		data.internal = internal;
		ui_feedlist_do_for_all_data(ptr, ACTION_FILTER_CHILDREN, append_node_tag, (gpointer)&data);
	} else {
		feedPtr fp = (feedPtr)ptr;
		const gchar *type = feed_type_fhp_to_str(feed_get_fhp(fp));
		gchar *interval = g_strdup_printf("%d",feed_get_update_interval(fp));
		gchar *cacheLimit = NULL;

		childNode = xmlNewChild(cur, NULL, BAD_CAST"outline", NULL);

		xmlNewProp(childNode, BAD_CAST"text", BAD_CAST feed_get_title(fp)); /* The OPML spec requires "text" */
		xmlNewProp(childNode, BAD_CAST"title", BAD_CAST feed_get_title(fp));
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
				
			xmlNewProp(childNode, BAD_CAST"id", BAD_CAST feed_get_id(fp));
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
			if(fp->sortColumn == IS_TITLE)
				xmlNewProp(childNode, BAD_CAST"sortColumn", BAD_CAST"title");
			else if(fp->sortColumn == IS_TIME)
				xmlNewProp(childNode, BAD_CAST"sortColumn", BAD_CAST"time");
			if(FALSE == fp->sortReversed)
				xmlNewProp(childNode, BAD_CAST"sortReversed", BAD_CAST"false");
				
			if(TRUE == feed_get_two_pane_mode(fp))
				xmlNewProp(childNode, BAD_CAST"twoPane", BAD_CAST"true");
		}
		
		/* add vfolder rules */
		iter = fp->rules;
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
		
		debug6(DEBUG_CONF, "adding feed: title=%s type=%s source=%d id=%s interval=%s cacheLimit=%s", feed_get_title(fp), type, feed_get_source(fp), feed_get_id(fp), interval, cacheLimit);
		g_free(cacheLimit);
		g_free(interval);
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
	if (str == NULL)
		return def;
	if (0==(sscanf(str,"%ld",&num)))
		num = def;
	
	return num;
}

/** 
 * called by import_parse_outline to parse all children outline tags
 * as vfolder rule descriptions 
 */
static void import_parse_children_as_rules(xmlNodePtr cur, feedPtr vp) {
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
					debug3(DEBUG_CACHE, "loading rule \"%s\" \"%s\" \"%s\"\n", type, ruleId, value);

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

static void import_parse_outline(xmlNodePtr cur, folderPtr folder, gboolean trusted) {
	gchar		*cacheLimitStr, *filter, *intervalStr, *lastPollStr, *htmlUrlStr, *sortStr;
	gchar		*title, *source, *typeStr, *tmp;
	feedPtr		fp = NULL;
	folderPtr	child;
	gboolean	dontParseChildren = FALSE;
	gint		interval;
	gchar		*id = NULL;
	
	debug_enter("import_parse_outline");
	
	/* process the outline node */	
	title = xmlGetProp(cur, BAD_CAST"title");
	if(title == NULL || !xmlStrcmp(title, BAD_CAST"")) {
		if(title != NULL)
			xmlFree(title);
		title = xmlGetProp(cur, BAD_CAST"text");
	}
	
	if(NULL == (source = xmlGetProp(cur, BAD_CAST"xmlUrl")))
		source = xmlGetProp(cur, BAD_CAST"xmlurl");	/* e.g. for AmphetaDesk */
	
	if(NULL != source) { /* Reading a feed */
		/* check if source is a help feed... */
		if(0 == strncmp(source, HELP1URL_1, strlen(HELP1URL_1))) {
			/* If it is we rebuild it with the current version 
			   just to be sure it is up-to-date. Of course this 
			   means there must be no other feeds under the
			   URL given by HELP1URL!!! */
			xmlFree(source);
			source = g_strdup_printf("%s%s%s", HELP1URL_1, VERSION, HELP1URL_2);
		}
	
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
			fp = vfolder_new();
			import_parse_children_as_rules(cur, fp);
		} else {
			fp = feed_new();
		}
		fp->fhp = feed_type_str_to_fhp(typeStr);

		/* Set the cache limit */
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

		/* sorting order */
		sortStr = xmlGetProp(cur, BAD_CAST"sortColumn");
		if(sortStr != NULL) {
			if(!xmlStrcmp(sortStr, "title"))
				fp->sortColumn = IS_TITLE;
			else if(!xmlStrcmp(sortStr, "time"))
				fp->sortColumn = IS_TIME;
			xmlFree(sortStr);
		}
		sortStr = xmlGetProp(cur, BAD_CAST"sortReversed");
		if(sortStr != NULL && !xmlStrcmp(sortStr, BAD_CAST"false"))
			fp->sortReversed = FALSE;
		if(sortStr != NULL)
			xmlFree(sortStr);
			
		
		tmp = xmlGetProp(cur, BAD_CAST"twoPane");
		if(NULL != tmp && !xmlStrcmp(tmp, BAD_CAST"true"))
			feed_set_two_pane_mode(fp, TRUE);
			
		/* set feed properties available from the OPML feed list 
		   they may be overwritten by the values of the cache file
		   but we need them in case the cache file loading fails */
		
		feed_set_source(fp, source);
		feed_set_filter(fp, filter);
		feed_set_title(fp, title);
		feed_set_update_interval(fp, interval);
		debug6(DEBUG_CONF, "loading feed: title=%s source=%s typeStr=%s id=%s interval=%d lastpoll=%ld", title, source, typeStr, id, interval, fp->lastPoll.tv_sec);
		
		if(id != NULL) {
			feed_set_id(fp, id);
			xmlFree(id);
			/* don't load here, because it's not sure that all vfolders are loaded */
		} else {
			id = conf_new_id();
			feed_set_id(fp, id);
			debug1(DEBUG_CONF, "seems to be an import, setting new id: %s and doing first download...", id);
			g_free(id);			
			if(FST_FEED == fp->type)	/* don't update vfolders */
				feed_schedule_update(fp, (xmlHasProp(cur, BAD_CAST"updateInterval") ? 0 : FEED_REQ_RESET_UPDATE_INT)
				                         | FEED_REQ_DOWNLOAD_FAVICON
				                         | FEED_REQ_AUTH_DIALOG);
		}

		ui_feedlist_add(folder, (nodePtr)fp, -1);
		
		if(source != NULL)
			xmlFree(source);
		if (filter != NULL)
			xmlFree(filter);
		xmlFree(typeStr);
		
	} else { /* It is a folder */
		debug1(DEBUG_CONF, "adding folder \"%s\"", title);
		child = restore_folder(folder, title, NULL, FST_FOLDER);
		g_assert(NULL != child);
		ui_feedlist_add(folder, (nodePtr)child, -1);
		folder = child;

		if(NULL != xmlHasProp(cur, BAD_CAST"expanded"))
			ui_folder_set_expansion(folder, TRUE);
		if(NULL != xmlHasProp(cur, BAD_CAST"collapsed"))
			ui_folder_set_expansion(folder, FALSE);
	}
	if(title != NULL)
		xmlFree(title);

	if(!dontParseChildren) {
		/* process any children */
		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
				import_parse_outline(cur, folder, trusted);
		
				cur = cur->next;				
		}
	}
	
	debug_exit("import_parse_outline");
}

static void import_parse_body(xmlNodePtr n, folderPtr parent, gboolean trusted) {
	xmlNodePtr cur;
	
	cur = n->xmlChildrenNode;
	while(cur != NULL) {
		if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
			import_parse_outline(cur, parent, trusted);
		cur = cur->next;
	}
}

static void import_parse_OPML(xmlNodePtr n, folderPtr parent, gboolean trusted) {
	xmlNodePtr cur;
	
	cur = n->xmlChildrenNode;
	while(cur != NULL) {
		/* we ignore the head */
		if((!xmlStrcmp(cur->name, BAD_CAST"body"))) {
			import_parse_body(cur, parent, trusted);
		}
		cur = cur->next;
	}	
}

/**
 * Used to process feeds directly after feed list loading.
 * Loads the given feed or requests a download. During feed
 * loading its items are automatically checked against all 
 * vfolder rules.
 */
static void import_initial_load_feed(feedPtr fp) {

	if(!feed_load(fp))
		feed_schedule_update(fp, 0);
	feed_unload(fp);
}

void import_OPML_feedlist(const gchar *filename, folderPtr parent, gboolean showErrors, gboolean trusted) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	
	debug1(DEBUG_CONF, "Importing OPML file: %s", filename);
	
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
						import_parse_OPML(cur, parent, trusted);
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
	
	/* load all feeds and by doing so automatically load all vfolders */
	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, import_initial_load_feed);
	ui_feedlist_update();
}


/* UI stuff */

void on_import_activate_cb(const gchar *filename, gpointer user_data) {
	folderPtr folder;
	
	if (filename != NULL) {
		
		folder = restore_folder(NULL, _("Imported feed list"), NULL, FST_FOLDER);
		
		/* add the new folder to the model */
		ui_feedlist_add(NULL, (nodePtr)folder, -1);
		
		import_OPML_feedlist(filename, folder, TRUE, FALSE);
	}
}

void on_import_activate(GtkMenuItem *menuitem, gpointer user_data) {

	ui_choose_file(_("Import Feed List"), GTK_WINDOW(mainwindow), _("Import"), FALSE, on_import_activate_cb, NULL, NULL);
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
	
	ui_choose_file(_("Export Feed List"), GTK_WINDOW(mainwindow), _("Export"), TRUE, on_export_activate_cb, NULL, NULL);
}

