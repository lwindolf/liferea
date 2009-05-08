/**
 * @file script.c LUA scripting implementation
 *
 * Copyright (C) 2006-2008 Lars Lindner <lars.lindner@gmail.com>
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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "liferea_wrap.h"
#include "script.h"

static lua_State *luaVM = NULL;

static void
lua_init (void)
{
	int i;

	luaVM = lua_open ();
	
	luaL_openlibs (luaVM);	/* LUA 5.1 allows loading all default modules */
	
	luaL_reg lualibs[] = {
		/* This loads swig generated Liferea module... */
		{SWIG_name,	SWIG_init},
		{NULL,		NULL}
	};
	
	for (i=0; lualibs[i].func != 0 ; i++) {
		lua_pushcfunction (luaVM, lualibs[i].func);
		lua_pushstring (luaVM, lualibs[i].name);
		lua_call (luaVM, 1, 0);
	}
}

static void
lua_run_cmd (const gchar *cmd)
{
	int	result;

	lua_getglobal(luaVM, "liferea");
	lua_pushnil(luaVM);
	lua_setfield(luaVM, -2, "calling_hook");
	lua_pop(luaVM, 1);

	result = luaL_dostring (luaVM, cmd);

	if (0 != result)
		g_warning ("Error code %d when running LUA command \"%s\"!", result, cmd);
}

static void
lua_run_script (const gchar *filename, const gchar *for_hook)
{
	int	result;

	lua_getglobal(luaVM, "liferea");
	if (for_hook != NULL)
	{
		lua_pushstring(luaVM, for_hook);
	}
	else
	{
		lua_pushnil(luaVM);
	}
	lua_setfield(luaVM, -2, "calling_hook");
	lua_pop(luaVM, 1);

	result = luaL_dofile (luaVM, filename);

	if (0 != result)
		g_warning ("Error code %d when running LUA script \"%s\"!", result, filename);
}

static void
lua_deinit (void)
{
	lua_close (luaVM);
}

struct scriptSupportImpl lua_script_impl = {
	"LUA",
	lua_init,
	lua_deinit,
	lua_run_cmd,
	lua_run_script,
};
