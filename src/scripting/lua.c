/**
 * @file script.c LUA scripting plugin implementation
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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "liferea_wrap.h"
#include "plugin.h"
#include "script.h"

static lua_State *luaVM = NULL;

static void lua_init(void) {
	int i;

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
}

static void lua_run_cmd(const gchar *cmd) {

	lua_dostring(luaVM, cmd);
}

static void lua_run_script(const gchar *filename) {

	lua_dofile(luaVM, filename);
}

static void lua_deinit(void) {

	lua_close(luaVM);
}

static struct scriptSupportImpl ssi = {
	SCRIPT_SUPPORT_API_VERSION,
	"LUA",
	lua_init,
	lua_deinit,
	lua_run_cmd,
	lua_run_script,
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"LUA Scripting Support Plugin",
	PLUGIN_TYPE_SCRIPT_SUPPORT,
	&ssi
};

DECLARE_PLUGIN(pi);
DECLARE_SCRIPT_SUPPORT_IMPL(ssi);
