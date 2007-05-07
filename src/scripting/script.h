/**
 * @file script.h scripting support interface
 *
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _SCRIPT_H
#define _SCRIPT_H

#include <gtk/gtk.h>

/** scripting support plugin interface */
typedef struct scriptSupportImpl {
	guint		api_version;				/**< API version of scripting support plugin */
	gchar		*name;					/**< descriptive name of the plugin */
	void		(*init)		(void);			/**< called on startup */
	void		(*deinit)	(void);			/**< called on shutdown */
	void		(*run_cmd)	(const gchar *cmd);	/**< runs the given command */
	void		(*run_script)	(const gchar *file);	/**< runs the given script */
} *scriptSupportImplPtr;

#define SCRIPT_SUPPORT_API_VERSION 1

#define DECLARE_SCRIPT_SUPPORT_IMPL(impl) \
	G_MODULE_EXPORT scriptSupportImplPtr script_support_impl_get_info() { \
		return &impl; \
	}
	
/* script support interface */
	
typedef enum hooks {
	SCRIPT_HOOK_INVALID = 0,
	SCRIPT_HOOK_STARTUP,
	
	/* update events */
	SCRIPT_HOOK_FEED_UPDATED,
	
	/* selection hooks */
	SCRIPT_HOOK_ITEM_SELECTED,
	SCRIPT_HOOK_FEED_SELECTED,
	SCRIPT_HOOK_ITEM_UNSELECT,
	SCRIPT_HOOK_FEED_UNSELECT,
	
	SCRIPT_HOOK_SHUTDOWN,
	
	SCRIPT_HOOK_NEW_SUBSCRIPTION
} hookType;

/**
 * Checks wether scripting support is available and enabled or not.
 *
 * @returns TRUE if scripting support is available
 */
gboolean script_support_enabled(void);

/**
 * Sets up the scripting engine.
 */
void script_init(void);

/**
 * Runs a single command line.
 *
 * @param cmd		the command string
 */
void script_run_cmd(const gchar *cmd);

/**
 * Run a single script from the script repository.
 *
 * @param name		the script name
 */
void script_run(const gchar *name);

/**
 * Run all scripts defined for the given hook id.
 *
 * @param type	scripting hook type
 */
void script_run_for_hook(hookType type);

/**
 * Adds a script to the script list of the given hook type.
 *
 * @param type		scripting hook type
 * @param scriptname	the script
 */
void script_hook_add(hookType type, const gchar *scriptname);

/**
 * Removes a script from the script list of the given hook type.
 * Frees the passed script name.
 *
 * @param type		scripting hook type
 * @param scriptname	the script
 */
void script_hook_remove(hookType type, gchar *scriptname);

/**
 * Returns the script name list for the given hook type.
 *
 * @returns script list
 */
GSList *script_hook_get_list(hookType type);

/**
 * Close down scripting.
 */
void script_deinit(void);

#endif
