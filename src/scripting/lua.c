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
		#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 501
		{"package", luaopen_package},
		#else
		{"package", luaopen_loadlib},
		#endif
		{SWIG_name,	SWIG_init},
		{NULL,		NULL}
	};
	
	for(i=0; lualibs[i].func != 0 ; i++) {
		lua_pushcfunction(luaVM, lualibs[i].func);
		lua_pushstring(luaVM, lualibs[i].name);
		lua_call(luaVM, 1, 0);
	}
}

static void lua_run_cmd(const gchar *cmd) {
	#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 501
	luaL_dostring(luaVM, cmd);
	#else
	lua_dostring(luaVM, cmd);
	#endif
}

static void lua_run_script(const gchar *filename) {
	#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >=501
	luaL_dofile(luaVM, filename);
	#else
	lua_dofile(luaVM, filename);
	#endif
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
