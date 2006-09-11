/**
 * @file script.c scripting support implementation
 *
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "liferea_wrap.h"
#endif

#include <glib.h>
#include "common.h"
#include "debug.h"
#include "script.h"

#ifdef USE_LUA
static lua_State *luaVM = NULL;
#endif

/** hash of the scripts registered for different hooks */
static GHashTable *scripts = NULL;
static gboolean scriptConfigLoading = FALSE;

static void script_config_load_hook_script(xmlNodePtr match, gpointer user_data) {
	gint	type = GPOINTER_TO_INT(user_data);
	gchar	*name;
	
	name = xmlNodeListGetString(match->doc, match->xmlChildrenNode, 1);
	if(name) {
		script_hook_add(type, name);
		g_free(name);
	}
}

static void script_config_load_hook(xmlNodePtr match, gpointer user_data) {
	gint	type;
	gchar	*tmp;
	
	tmp = xmlGetProp(match, BAD_CAST"type");
	type = atoi(tmp);
	g_free(tmp);
	if(!type)
		return;
	common_xpath_foreach_match(match, "script", script_config_load_hook_script, GINT_TO_POINTER(type));
}

static void script_config_load(void) {
	xmlDocPtr	doc;
	gchar		*filename, *data;
	gint		len;

	scriptConfigLoading = TRUE;
	
	filename = common_create_cache_filename(NULL, "scripts", "xml");	
	if(g_file_get_contents(filename, &data, &len, NULL)) {
		doc = common_parse_xml(data, len, FALSE, NULL);
		if(doc) {
			common_xpath_foreach_match(xmlDocGetRootElement(doc),
			                           "/scripts/hook",
						   script_config_load_hook,
						   NULL);
			xmlFreeDoc(doc);		
		}
	}
	
	scriptConfigLoading = FALSE;
}

static void script_config_save_hook(gpointer key, gpointer value, gpointer user_data) {
	xmlNodePtr	hookNode, rootNode = (xmlNodePtr)user_data;
	GSList		*list = (GSList *)value;
	gchar		*tmp;
	
	tmp = g_strdup_printf("%d", GPOINTER_TO_INT(key));
	hookNode = xmlNewChild(rootNode, NULL, "hook", NULL);
	xmlNewProp(hookNode, BAD_CAST"type", BAD_CAST tmp);
	g_free(tmp);
	 
	while(list) {
		xmlNewTextChild(hookNode, NULL, "script", (gchar *)list->data);
		list = g_slist_next(list);
	}
}

static void script_config_save(void) {
	xmlDocPtr	doc = NULL;
	xmlNodePtr	rootNode;
	gchar		*filename;
	
	if(scriptConfigLoading)
		return;
	
	doc = xmlNewDoc("1.0");
	rootNode = xmlDocGetRootElement(doc);
	rootNode = xmlNewDocNode(doc, NULL, "scripts", NULL);
	xmlDocSetRootElement(doc, rootNode);
	
	g_hash_table_foreach(scripts, script_config_save_hook, (gpointer)rootNode);

	filename = common_create_cache_filename(NULL, "scripts", "xml");
	if(-1 == xmlSaveFormatFileEnc(filename, doc, NULL, 1))
		g_warning("Could save script config %s!", filename);
		
	xmlFree(doc);
	g_free(filename);
}

void script_init(void) {
	int i;

	scripts = g_hash_table_new(g_direct_hash, g_direct_equal);
	script_config_load();

#ifdef USE_LUA	
	luaVM = lua_open();
	
	luaL_reg lualibs[] = {
		{"base",	luaopen_base},
		{"table",	luaopen_table},
		{"io",		luaopen_io}, 
		{"string",	luaopen_string},
		{"math",	luaopen_math},
		{"debug",	luaopen_debug},
		{SWIG_name,	SWIG_init},
		{NULL,		NULL}
	};
	
	for(i=0; lualibs[i].func != 0 ; i++) {
		lualibs[i].func(luaVM);  /* open library */
		lua_settop(luaVM, 0);  /* discard any results */
	}
#else
	debug0(DEBUG_PLUGINS, "LUA scripting not supported.");
#endif
}

void script_run_cmd(const gchar *cmd) {

#ifdef USE_LUA
	lua_dostring(luaVM, cmd);
#endif
}

void script_run(const gchar *name) {
	gchar	*filename;
	
#ifdef USE_LUA
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "scripts", name, "lua");
	lua_dofile(luaVM, filename);
	g_free(filename);
#endif	
}

void script_run_for_hook(hookType type) {
	GSList	*hook;

	hook = g_hash_table_lookup(scripts, GINT_TO_POINTER(type));
	while(hook) {
		script_run((gchar *)hook->data);
		hook = g_slist_next(hook);
	}
}

void script_hook_add(hookType type, const gchar *scriptname) {
	GSList	*hook;

	hook = g_hash_table_lookup(scripts, GINT_TO_POINTER(type));
	hook = g_slist_append(hook, g_strdup(scriptname));
	g_hash_table_insert(scripts, GINT_TO_POINTER(type), hook);
	script_config_save();
}

void script_hook_remove(hookType type, gchar *scriptname) {
	GSList	*hook;

	hook = g_hash_table_lookup(scripts, GINT_TO_POINTER(type));
	hook = g_slist_remove(hook, scriptname);
	g_hash_table_insert(scripts, GINT_TO_POINTER(type), hook);
	script_config_save();
	g_free(scriptname);
}

GSList *script_hook_get_list(hookType type) {

	return g_hash_table_lookup(scripts, GINT_TO_POINTER(type));
}

void script_deinit(void) {

	// FIXME: foreach hook free script list
	g_hash_table_destroy(scripts);

#ifdef USE_LUA
	lua_close(luaVM);
#endif
}
