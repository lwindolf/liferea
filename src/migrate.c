/**
 * @file migrate.c migration between different cache versions
 * 
 * Copyright (C) 2007-2011  Lars Windolf <lars.lindner@gmail.com>
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

#include "migrate.h"

#include <glib.h>
#include <glib/gstdio.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "ui/ui_common.h"

#define LIFEREA_CURRENT_DIR ".liferea_1.8"

static void 
migrate_copy_dir (const gchar *from,
                  const gchar *to,
                  const gchar *subdir) 
{
	gchar *fromDirname, *toDirname;
	gchar *srcfile, *destfile;
   	GDir *dir;
		
	fromDirname = g_build_filename (g_get_home_dir (), from, subdir, NULL);
	toDirname = g_build_filename (g_get_home_dir (), to, subdir, NULL);
	
	dir = g_dir_open (fromDirname, 0, NULL);
	while (NULL != (srcfile = (gchar *)g_dir_read_name (dir))) {
		destfile = g_build_filename (toDirname, srcfile, NULL);
		srcfile = g_build_filename (fromDirname, srcfile, NULL);
		if (g_file_test (srcfile, G_FILE_TEST_IS_REGULAR)) {
			g_print ("copying %s\n     to %s\n", srcfile, destfile);
			common_copy_file (srcfile, destfile);
		} else {
			g_print("skipping %s\n", srcfile);
		}
		g_free (destfile);
		g_free (srcfile);
	}
	g_dir_close(dir);
	
	g_free (fromDirname);
	g_free (toDirname);
}

static void
migrate_from_14plus (const gchar *olddir, nodePtr node)
{
	gchar *filename;

	// FIXME: Use XDG dirs for 1.9 cache structure
	g_print("Performing %s -> %s cache migration...\n", olddir, LIFEREA_CURRENT_DIR);	
	
	/* 1.) Close already loaded DB */
	db_deinit ();

	/* 2.) Copy all files */
	migrate_copy_dir (olddir, LIFEREA_CURRENT_DIR, "");
	migrate_copy_dir (olddir, LIFEREA_CURRENT_DIR, "cache" G_DIR_SEPARATOR_S "favicons");
	migrate_copy_dir (olddir, LIFEREA_CURRENT_DIR, "cache" G_DIR_SEPARATOR_S "scripts");
	migrate_copy_dir (olddir, LIFEREA_CURRENT_DIR, "cache" G_DIR_SEPARATOR_S "plugins");	
	
	/* 3.) And reopen the copied DB */
	db_init ();

	/* 4.) Migrate file feed list into DB */
	filename = common_create_cache_filename(NULL, "feedlist", "opml");

	if(!import_OPML_feedlist (filename, node, FALSE, TRUE))
		g_error ("Fatal: Feed list migration failed!");

	g_free(filename);

	ui_show_info_box (_("This version of Liferea uses a new cache format and has migrated your "
	                    "feed cache. The cache content in %s was not deleted automatically. "
			    "Please remove this directory manually once you are sure migration was successful!"), olddir);
}

void
migration_execute (migrationMode mode, nodePtr node)
{
	switch (mode) {
		case MIGRATION_FROM_14:
			migrate_from_14plus (".liferea_1.4", node);
			break;
		case MIGRATION_FROM_16:
			migrate_from_14plus (".liferea_1.6", node);
			break;
		case MIGRATION_FROM_18:
			migrate_from_14plus (".liferea_1.8", node);
			break;
		case MIGRATION_MODE_INVALID:
		default:
			g_error ("Invalid migration mode!");
			return;
			break;
	}
}
