/**
 * @file migrate.c migration between different cache versions
 * 
 * Copyright (C) 2007-2012  Lars Windolf <lars.windolf@gmx.de>
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
#include <gio/gio.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "export.h"

/**
 * Copy a from $HOME/<from>/subdir to a target directory <to>/subdir.
 *
 * @param from		relative base path in $HOME (e.g. ".liferea_1.4")
 * @param to		absolute target base path (e.g. "/home/joe/.config")
 * @param subdir	subdir to copy from source to destination (can be empty string)
 */
static void 
migrate_copy_dir (const gchar *from,
                  const gchar *to,
                  const gchar *subdir) 
{
	gchar *fromDirname, *toDirname;
	gchar *srcfile, *destfile;
   	GDir *dir;

	g_print ("Processing %s%s...\n", from, subdir);
		
	fromDirname = g_build_filename (g_get_home_dir (), from, subdir, NULL);
	toDirname = g_build_filename (to, subdir, NULL);
	
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
migrate_from_14plus (const gchar *oldBaseDir, nodePtr node)
{
	GFile *sourceDbFile, *targetDbFile;
	gchar *newConfigDir, *newCacheDir, *newDataDir, *oldCacheDir, *filename;

	g_print("Performing %s -> XDG cache migration...\n", oldBaseDir);	
	
	/* 1.) Close already loaded DB */
	db_deinit ();

	/* 2.) Copy all files */
	newCacheDir	= g_build_filename (g_get_user_cache_dir(), "liferea", NULL);
	newConfigDir	= g_build_filename (g_get_user_config_dir(), "liferea", NULL);
	newDataDir	= g_build_filename (g_get_user_data_dir(), "liferea", NULL);
	oldCacheDir	= g_build_filename (oldBaseDir, "cache", NULL);

	migrate_copy_dir (oldBaseDir, newConfigDir, "");
	migrate_copy_dir (oldCacheDir, newCacheDir, G_DIR_SEPARATOR_S "favicons");
	migrate_copy_dir (oldCacheDir, newCacheDir, G_DIR_SEPARATOR_S "plugins");	

	/* 3.) Move DB to from new config dir to cache dir instead (this is
	       caused by the batch copy in step 2.) */
	sourceDbFile = g_file_new_for_path (g_build_filename (newConfigDir, "liferea.db", NULL));
	targetDbFile = g_file_new_for_path (g_build_filename (newDataDir, "liferea.db", NULL));
	g_file_move (sourceDbFile, targetDbFile, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL);
	g_object_unref (sourceDbFile);
	g_object_unref (targetDbFile);
	
	/* 3.) And reopen the copied DB */
	db_init ();

	/* 4.) Migrate file feed list into DB */
	filename = common_create_config_filename ("feedlist.opml");

	if (!import_OPML_feedlist (filename, node, FALSE, TRUE))
		g_error ("Fatal: Feed list migration failed!");

	g_free (filename);
	g_free (newConfigDir);
	g_free (newCacheDir);
	g_free (oldCacheDir);
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
