/**
 * @file db.c sqlite backend
 *
 * Copyright (C) 2007-2020  Lars Windolf <lars.windolf@gmx.de>
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

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "item.h"
#include "itemset.h"
#include "metadata.h"
#include "vfolder.h"

static sqlite3	*db = NULL;
gboolean searchFolderRebuild = FALSE;

/** hash of all prepared statements */
static GHashTable *statements = NULL;

static void db_view_remove (const gchar *id);

static void
db_prepare_stmt (sqlite3_stmt **stmt, const gchar *sql)
{
	gint		res;
	const char	*left;

	res = sqlite3_prepare_v2 (db, sql, -1, stmt, &left);
	if ((SQLITE_BUSY == res) ||
	    (SQLITE_LOCKED == res)) {
		g_warning ("The Liferea cache DB seems to be used by another instance (error code=%d)! Only one accessing instance is allowed.", res);
	    	exit(1);
	}
	if (SQLITE_OK != res)
		g_error ("Failure while preparing statement, (error=%d, %s) SQL: \"%s\"", res, sqlite3_errmsg(db), sql);
}

static void
db_new_statement (const gchar *name, const gchar *sql)
{

	if (!statements)
		statements = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_insert (statements, (gpointer)name, (gpointer)sql);
}

static sqlite3_stmt *
db_get_statement (const gchar *name)
{
	sqlite3_stmt *statement;
	gchar *sql;

	sql = (gchar *) g_hash_table_lookup (statements, name);

	db_prepare_stmt (&statement, sql);

	if (!statement)
		g_error ("Fatal: unknown prepared statement \"%s\" requested!", name);

	sqlite3_reset (statement);
	return statement;
}

static void
db_exec (const gchar *sql)
{
	gchar	*err;
	gint	res;

	debug1 (DEBUG_DB, "executing SQL: %s", sql);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (1 >= res) {
		debug2 (DEBUG_DB, " -> result: %d (%s)", res, err?err:"success");
	} else {
		g_warning ("Unexpected status on SQL execution: %d (%s)", res, err?err:"success");
	}
	sqlite3_free (err);
}

static gboolean
db_table_exists (const gchar *name)
{
	gchar		*sql;
	sqlite3_stmt	*stmt;
	gint		res = 0;

	sql = sqlite3_mprintf ("SELECT COUNT(type) FROM sqlite_master WHERE type = 'table' AND name = '%s';", name);
	db_prepare_stmt (&stmt, sql);
	sqlite3_reset (stmt);
	if (SQLITE_ROW == sqlite3_step (stmt))
		res = sqlite3_column_int (stmt, 0);

	sqlite3_finalize (stmt);
	sqlite3_free (sql);
	return (1 == res);
}

static void
db_set_schema_version (gint schemaVersion)
{
	gchar	*err, *sql;

	sql = sqlite3_mprintf ("REPLACE INTO info (name, value) VALUES ('schemaVersion',%d);", schemaVersion);
	if (SQLITE_OK != sqlite3_exec (db, sql, NULL, NULL, &err))
		debug1 (DEBUG_DB, "setting schema version failed: %s", err);
	sqlite3_free (sql);
	sqlite3_free (err);
}

static gint
db_get_schema_version (void)
{
	guint		schemaVersion = 0;
	sqlite3_stmt	*stmt;

	if (!db_table_exists ("info")) {
		db_exec ("CREATE TABLE info ( "
		         "   name	TEXT, "
			 "   value	TEXT, "
		         "   PRIMARY KEY (name) "
		         ");");
		db_set_schema_version (-1);
	}

	db_prepare_stmt (&stmt, "SELECT value FROM info WHERE name = 'schemaVersion'");
	if (SQLITE_ROW == sqlite3_step (stmt))
		schemaVersion = sqlite3_column_int (stmt, 0);
	sqlite3_finalize (stmt);

	return schemaVersion;
}

static void
db_begin_transaction (void)
{
	gchar	*sql, *err;
	gint	res;

	sql = sqlite3_mprintf ("BEGIN");
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_warning ("Transaction begin failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);
}

static void
db_end_transaction (void)
{
	gchar	*sql, *err;
	gint	res;

	sql = sqlite3_mprintf ("END");
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_warning ("Transaction end failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);
}

#define VACUUM_ON_FRAGMENTATION_RATIO	10

static void
db_vacuum (void)
{
	sqlite3_stmt	*stmt;
	gint		res, page_count, freelist_count;

	/* Determine fragmentation ratio using

		PRAGMA page_count
		PRAGMA freelist_count

	   as suggested by adriatic in this blog post
	   http://jeff.ecchi.ca/blog/2011/12/24/investigating-lifereas-startup-performance/#comment-19989
	   and perform VACUUM only when needed.
	 */

	db_prepare_stmt (&stmt, "PRAGMA page_count");
	sqlite3_reset (stmt);
	res = sqlite3_step (stmt);
	if (SQLITE_ROW != res)
		g_error ("Could not determine page count (error code %d)!", res);
	page_count = sqlite3_column_int (stmt, 0);
	sqlite3_finalize (stmt);

	db_prepare_stmt (&stmt, "PRAGMA freelist_count");
	sqlite3_reset (stmt);
	res = sqlite3_step (stmt);
	if (SQLITE_ROW != res)
		g_error ("Could not determine free list count (error code %d)!", res);
	freelist_count = sqlite3_column_int (stmt, 0);
	sqlite3_finalize (stmt);

	float fragmentation = (100 * (float)freelist_count/page_count);
	if (fragmentation > VACUUM_ON_FRAGMENTATION_RATIO) {
		debug2 (DEBUG_DB, "Performing VACUUM as freelist count/page count ratio %2.2f > %d",
		                  fragmentation, VACUUM_ON_FRAGMENTATION_RATIO);
		debug_start_measurement (DEBUG_DB);
		db_exec ("VACUUM;");
		debug_end_measurement (DEBUG_DB, "VACUUM");
	} else {
		debug2 (DEBUG_DB, "No VACUUM as freelist count/page count ratio %2.2f <= %d",
		                  fragmentation, VACUUM_ON_FRAGMENTATION_RATIO);
	}
}

static void
db_open (void)
{
	gchar	*filename;
	gint	res;

	filename = common_create_data_filename ("liferea.db");
	debug1 (DEBUG_DB, "Opening DB file %s...", filename);
	res = sqlite3_open_v2 (filename, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (SQLITE_OK != res)
		g_error ("Data base file %s could not be opened (error code %d: %s)...", filename, res, sqlite3_errmsg (db));
	g_free (filename);

	sqlite3_extended_result_codes (db, TRUE);

	db_exec("PRAGMA journal_mode=WAL");
	db_exec("PRAGMA page_size=32768");
	db_exec("PRAGMA synchronous=NORMAL");
}

#define SCHEMA_TARGET_VERSION 10

/* opening or creation of database */
void
db_init (void)
{
	gint		res;

	debug_enter ("db_init");

	db_open ();

	/* create info table/check versioning info */
	debug1 (DEBUG_DB, "current DB schema version: %d", db_get_schema_version ());

	if (-1 == db_get_schema_version ()) {
		/* no schema version available -> first installation without tables... */
		db_set_schema_version (SCHEMA_TARGET_VERSION);
		/* nothing exists yet, tables will be created below */
	}

	if (SCHEMA_TARGET_VERSION < db_get_schema_version ())
		g_error ("Fatal: The cache database was created by a newer version of Liferea than this one!");

	if (SCHEMA_TARGET_VERSION > db_get_schema_version ()) {
		/* do table migration */
		if (db_get_schema_version () < 5)
			g_error ("This version of Liferea doesn't support migrating from such an old DB file!");

		if (db_get_schema_version () == 5 || db_get_schema_version () == 6) {
			debug0 (DEBUG_DB, "dropping triggers in preparation of database migration");
			db_exec ("BEGIN; "
			         "DROP TRIGGER item_removal; "
				 "DROP TRIGGER item_insert; "
				 "END;");
		}

		if (db_get_schema_version () == 5) {
			/* 1.4.9 -> 1.4.10 adding parent_item_id to itemset relation */
			debug0 (DEBUG_DB, "migrating from schema version 5 to 6 (this drops all comments)");
			db_exec ("BEGIN; "
			         "DELETE FROM itemsets WHERE comment = 1; "
				 "DELETE FROM items WHERE comment = 1; "
			         "CREATE TEMPORARY TABLE itemsets_backup(item_id,node_id,read,comment); "
				 "INSERT INTO itemsets_backup SELECT item_id,node_id,read,comment FROM itemsets; "
				 "DROP TABLE itemsets; "
				 "CREATE TABLE itemsets ("
		        	 "   item_id		INTEGER,"
				 "   parent_item_id     INTEGER,"
		        	 "   node_id		TEXT,"
		        	 "   read		INTEGER,"
				 "   comment            INTEGER,"
		        	 "   PRIMARY KEY (item_id, node_id)"
		        	 "); "
				 "INSERT INTO itemsets SELECT item_id,0,node_id,read,comment FROM itemsets_backup; "
				 "DROP TABLE itemsets_backup; "
				 "REPLACE INTO info (name, value) VALUES ('schemaVersion',6); "
				 "END;");
		}

		if (db_get_schema_version () == 6) {
			/* 1.4.15 -> 1.4.16 adding parent_node_id to itemset relation */
			debug0 (DEBUG_DB, "migrating from schema version 6 to 7 (this drops all comments)");
			db_exec ("BEGIN; "
			         "DELETE FROM itemsets WHERE comment = 1; "
				 "DELETE FROM items WHERE comment = 1; "
			         "CREATE TEMPORARY TABLE itemsets_backup(item_id,node_id,read,comment); "
				 "INSERT INTO itemsets_backup SELECT item_id,node_id,read,comment FROM itemsets; "
				 "DROP TABLE itemsets; "
				 "CREATE TABLE itemsets ("
		        	 "   item_id		INTEGER,"
				 "   parent_item_id     INTEGER,"
		        	 "   node_id		TEXT,"
				 "   parent_node_id     TEXT,"
		        	 "   read		INTEGER,"
				 "   comment            INTEGER,"
		        	 "   PRIMARY KEY (item_id, node_id)"
		        	 "); "
				 "INSERT INTO itemsets SELECT item_id,0,node_id,node_id,read,comment FROM itemsets_backup; "
				 "DROP TABLE itemsets_backup; "
				 "REPLACE INTO info (name, value) VALUES ('schemaVersion',7); "
				 "END;");
		}

		if (db_get_schema_version () == 7) {
			/* 1.7.1 -> 1.7.2 dropping the itemsets and attention_stats relation */
			db_exec ("BEGIN; "
			         "CREATE TEMPORARY TABLE items_backup("
			         "   item_id, "
			         "   title, "
			         "   read, "
			         "   updated, "
			         "   popup, "
			         "   marked, "
			         "   source, "
			         "   source_id, "
			         "   valid_guid, "
			         "   description, "
			         "   date, "
			         "   comment_feed_id, "
			         "   comment); "
			         "INSERT into items_backup SELECT ROWID, title, read, updated, popup, marked, source, source_id, valid_guid, description, date, comment_feed_id, comment FROM items; "
			         "DROP TABLE items; "
		                 "CREATE TABLE items ("
		        	 "   item_id		INTEGER,"
				 "   parent_item_id     INTEGER,"
		        	 "   node_id		TEXT,"
				 "   parent_node_id     TEXT,"
		        	 "   title		TEXT,"
		        	 "   read		INTEGER,"
		        	 "   updated		INTEGER,"
		        	 "   popup		INTEGER,"
		        	 "   marked		INTEGER,"
		        	 "   source		TEXT,"
		        	 "   source_id		TEXT,"
		        	 "   valid_guid		INTEGER,"
		        	 "   description	TEXT,"
		        	 "   date		INTEGER,"
		        	 "   comment_feed_id	INTEGER,"
				 "   comment            INTEGER,"
				 "   PRIMARY KEY (item_id)"
		        	 ");"
			         "INSERT INTO items SELECT itemsets.item_id, parent_item_id, node_id, parent_node_id, title, itemsets.read, updated, popup, marked, source, source_id, valid_guid, description, date, comment_feed_id, itemsets.comment FROM items_backup JOIN itemsets ON itemsets.item_id = items_backup.item_id; "
			         "DROP TABLE items_backup; "
			         "DROP TABLE itemsets; "
			         "REPLACE INTO info (name, value) VALUES ('schemaVersion',8); "
			         "END;" );

			db_exec ("DROP TABLE attention_stats");	/* this is unconditional, no checks and backups needed */
		}

		if (db_get_schema_version () == 8) {
			gchar *sql;
			sqlite3_stmt *stmt;

			/* 1.7.3 -> 1.7.4 change search folder handling */
			db_exec ("BEGIN; "
			         "DROP TABLE view_state; "
			         "DROP TABLE update_state; "
				 "CREATE TABLE search_folder_items ("
				 "   node_id            STRING,"
	         		 "   item_id		INTEGER,"
				 "   PRIMARY KEY (node_id, item_id)"
				 ");"
			         "REPLACE INTO info (name, value) VALUES ('schemaVersion',9); "
			         "END;" );

			debug0 (DEBUG_DB, "Removing all views.");
			sql = sqlite3_mprintf("SELECT name FROM sqlite_master WHERE type='view';");
			res = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
			sqlite3_free (sql);
			if (SQLITE_OK != res) {
				debug1 (DEBUG_DB, "Could not determine views (error=%d)", res);
			} else {
				sqlite3_reset (stmt);

					while (sqlite3_step (stmt) == SQLITE_ROW) {
						const gchar *viewName = (gchar *)sqlite3_column_text (stmt, 0) + strlen("view_");
						gchar *copySql = g_strdup_printf("INSERT INTO search_folder_items (node_id, item_id) SELECT '%s',item_id FROM view_%s;", viewName, viewName);

						db_exec (copySql);
						db_view_remove (viewName);

						g_free (copySql);
					}

				sqlite3_finalize (stmt);
			}
		}

		if (db_get_schema_version () == 9) {
			/* A parent node id to search folder relation to allow cleanups */
			db_exec ("BEGIN; "
			         "DROP TABLE search_folder_items; "
				 "CREATE TABLE search_folder_items ("
				 "   node_id            STRING,"
				 "   parent_node_id     STRING,"
	         		 "   item_id		INTEGER,"
				 "   PRIMARY KEY (node_id, item_id)"
				 ");"
			         "REPLACE INTO info (name, value) VALUES ('schemaVersion',10); "
			         "END;" );

			searchFolderRebuild = TRUE;
		}
	}

	if (SCHEMA_TARGET_VERSION != db_get_schema_version ())
		g_error ("Fatal: DB schema version not up-to-date! Running with --debug-db could give some hints about the problem!");

	/* Vacuuming... */

	db_vacuum ();

	/* Schema creation */

	debug_start_measurement (DEBUG_DB);
	db_begin_transaction ();

	/* 1. Create tables if they do not exist yet */
	db_exec ("CREATE TABLE items ("
        	 "   item_id		INTEGER,"
		 "   parent_item_id     INTEGER,"
        	 "   node_id		TEXT," /* FIXME: migrate node ids to real integers */
		 "   parent_node_id     TEXT," /* FIXME: migrate node ids to real integers */
        	 "   title		TEXT,"
        	 "   read		INTEGER,"
        	 "   updated		INTEGER,"
        	 "   popup		INTEGER,"
        	 "   marked		INTEGER,"
        	 "   source		TEXT,"
        	 "   source_id		TEXT,"
        	 "   valid_guid		INTEGER,"
        	 "   description	TEXT,"
        	 "   date		INTEGER,"
        	 "   comment_feed_id	TEXT,"
		 "   comment            INTEGER,"
		 "   PRIMARY KEY (item_id)"
        	 ");");

	db_exec ("CREATE INDEX items_idx ON items (source_id);");
	db_exec ("CREATE INDEX items_idx2 ON items (comment_feed_id);");
	db_exec ("CREATE INDEX items_idx3 ON items (node_id);");
	db_exec ("CREATE INDEX items_idx4 ON items (item_id);");
	db_exec ("CREATE INDEX items_idx5 ON items (parent_item_id);");
	db_exec ("CREATE INDEX items_idx6 ON items (parent_node_id);");

	db_exec ("CREATE TABLE metadata ("
        	 "   item_id		INTEGER,"
        	 "   nr              	INTEGER,"
        	 "   key             	TEXT,"
        	 "   value           	TEXT,"
        	 "   PRIMARY KEY (item_id, nr)"
        	 ");");

	db_exec ("CREATE INDEX metadata_idx ON metadata (item_id);");

	db_exec ("CREATE TABLE subscription ("
        	 "   node_id            STRING,"
		 "   source             STRING,"
		 "   orig_source        STRING,"
		 "   filter_cmd         STRING,"
		 "   update_interval	INTEGER,"
		 "   default_interval   INTEGER,"
		 "   discontinued       INTEGER,"
		 "   available          INTEGER,"
        	 "   PRIMARY KEY (node_id)"
		 ");");

	db_exec ("CREATE TABLE subscription_metadata ("
        	 "   node_id            STRING,"
		 "   nr                 INTEGER,"
		 "   key                TEXT,"
		 "   value              TEXT,"
		 "   PRIMARY KEY (node_id, nr)"
		 ");");

	db_exec ("CREATE INDEX subscription_metadata_idx ON subscription_metadata (node_id);");

	db_exec ("CREATE TABLE node ("
        	 "   node_id		STRING,"
        	 "   parent_id		STRING,"
        	 "   title		STRING,"
		 "   type		INTEGER,"
		 "   expanded           INTEGER,"
		 "   view_mode		INTEGER,"	// now unused
		 "   sort_column	INTEGER,"
		 "   sort_reversed	INTEGER,"
		 "   PRIMARY KEY (node_id)"
        	 ");");

	db_exec ("CREATE TABLE search_folder_items ("
	         "   node_id            STRING,"
	         "   parent_node_id     STRING,"
	         "   item_id		INTEGER,"
		 "   PRIMARY KEY (node_id, item_id)"
		 ");");

	db_end_transaction ();
	debug_end_measurement (DEBUG_DB, "table setup");

	/* 2. Removing old triggers */
	db_exec ("DROP TRIGGER item_insert;");
	db_exec ("DROP TRIGGER item_update;");
	db_exec ("DROP TRIGGER item_removal;");
	db_exec ("DROP TRIGGER subscription_removal;");

	/* 3. Cleanup of DB */

	/* Note: do not check on subscriptions here, as non-subscription node
	   types (e.g. news bin) do contain items too. */
	debug0 (DEBUG_DB, "Checking for items without a feed list node...\n");
	db_exec ("DELETE FROM items WHERE comment = 0 AND node_id NOT IN "
        	 "(SELECT node_id FROM node);");

        debug0 (DEBUG_DB, "Checking for comments without parent item...\n");
	db_exec ("BEGIN; "
	         "   CREATE TEMP TABLE tmp_id ( id );"
	         "   INSERT INTO tmp_id SELECT item_id FROM items WHERE comment = 1 AND parent_item_id NOT IN (SELECT item_id FROM items WHERE comment = 0);"
	         /* limit to 1000 items as it is very slow */
	         "   DELETE FROM items WHERE item_id IN (SELECT id FROM tmp_id LIMIT 1000);"
	         "   DROP TABLE tmp_id;"
		 "END;");

	debug0 (DEBUG_DB, "Checking for search folder items without a feed list node...\n");
	db_exec ("DELETE FROM search_folder_items WHERE parent_node_id NOT IN "
        	 "(SELECT node_id FROM node);");

	debug0 (DEBUG_DB, "Checking for search folder items without a search folder...\n");
	db_exec ("DELETE FROM search_folder_items WHERE node_id NOT IN "
        	 "(SELECT node_id FROM node);");

	debug0 (DEBUG_DB, "Checking for search folder with comments...\n");
	db_exec ("DELETE FROM search_folder_items WHERE comment = 1;");

	debug0 (DEBUG_DB, "Checking for subscription metadata without node...\n");
	db_exec ("DELETE FROM subscription_metadata WHERE node_id NOT IN "
          	 "(SELECT node_id FROM node);");

	debug0 (DEBUG_DB, "Removing metadata without item...\n");
	db_exec ("DELETE FROM metadata WHERE item_id NOT IN "
		 "(SELECT item_id FROM items);");

	debug0 (DEBUG_DB, "DB cleanup finished. Continuing startup.");

	/* 4. Creating triggers (after cleanup so it is not slowed down by triggers) */

	/* This trigger does explicitely not remove comments! */
	db_exec ("CREATE TRIGGER item_removal DELETE ON items "
        	 "BEGIN "
		 "   DELETE FROM metadata WHERE item_id = old.item_id; "
		 "   DELETE FROM search_folder_items WHERE item_id = old.item_id; "
        	 "END;");

	db_exec ("CREATE TRIGGER subscription_removal DELETE ON subscription "
        	 "BEGIN "
		 "   DELETE FROM node WHERE node_id = old.node_id; "
		 "   DELETE FROM subscription_metadata WHERE node_id = old.node_id; "
		 "   DELETE FROM search_folder_items WHERE parent_node_id = old.node_id; "
        	 "END;");

	/* Note: view counting triggers are set up in the view preparation code (see db_view_create()) */
	/* prepare statements */

	db_new_statement ("itemsetLoadStmt",
	                  "SELECT item_id FROM items WHERE node_id = ?");

	db_new_statement ("itemsetLoadOffsetStmt",
			  "SELECT item_id FROM items WHERE comment = 0 LIMIT ? OFFSET ?");

	db_new_statement ("itemsetReadCountStmt",
	                  "SELECT COUNT(item_id) FROM items "
		          "WHERE read = 0 AND node_id = ?");

	db_new_statement ("itemsetItemCountStmt",
	                  "SELECT COUNT(item_id) FROM items "
		          "WHERE node_id = ?");

	db_new_statement ("itemsetRemoveStmt",
	                  "DELETE FROM items WHERE item_id = ? OR (comment = 1 AND parent_item_id = ?)");

	db_new_statement ("itemsetRemoveAllStmt",
	                  "DELETE FROM items WHERE node_id = ? OR (comment = 1 AND parent_node_id = ?)");

	db_new_statement ("itemsetMarkAllPopupStmt",
	                  "UPDATE items SET popup = 0 WHERE node_id = ?");

	db_new_statement ("itemLoadStmt",
	                  "SELECT "
	                  "title,"
	                  "read,"
	                  "updated,"
	                  "popup,"
	                  "marked,"
	                  "source,"
	                  "source_id,"
	                  "valid_guid,"
	                  "description,"
	                  "date,"
		          "comment_feed_id,"
		          "comment,"
		          "item_id,"
			  "parent_item_id, "
		          "node_id, "
			  "parent_node_id "
	                  " FROM items WHERE item_id = ?");

	db_new_statement ("itemUpdateStmt",
	                  "REPLACE INTO items ("
	                  "title,"
	                  "read,"
	                  "updated,"
	                  "popup,"
	                  "marked,"
	                  "source,"
	                  "source_id,"
	                  "valid_guid,"
	                  "description,"
	                  "date,"
		          "comment_feed_id,"
		          "comment,"
	                  "item_id,"
	                  "parent_item_id,"
	                  "node_id,"
	                  "parent_node_id"
	                  ") values (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");

	db_new_statement ("itemStateUpdateStmt",
			  "UPDATE items SET read=?, marked=?, updated=? "
			  "WHERE item_id=?");

	db_new_statement ("duplicatesFindStmt",
	                  "SELECT item_id FROM items WHERE source_id = ?");

	db_new_statement ("duplicateNodesFindStmt",
	                  "SELECT node_id FROM items WHERE item_id IN "
			  "(SELECT item_id FROM items WHERE source_id = ?)");

	db_new_statement ("duplicatesMarkReadStmt",
 	                  "UPDATE items SET read = 1, updated = 0 WHERE source_id = ?");

	db_new_statement ("metadataLoadStmt",
	                  "SELECT key,value,nr FROM metadata WHERE item_id = ? ORDER BY nr");

	db_new_statement ("metadataUpdateStmt",
	                  "REPLACE INTO metadata (item_id,nr,key,value) VALUES (?,?,?,?)");

	db_new_statement ("subscriptionUpdateStmt",
	                  "REPLACE INTO subscription ("
			  "node_id,"
			  "source,"
			  "orig_source,"
			  "filter_cmd,"
			  "update_interval,"
			  "default_interval,"
			  "discontinued,"
			  "available"
			  ") VALUES (?,?,?,?,?,?,?,?)");

	db_new_statement ("subscriptionRemoveStmt",
	                  "DELETE FROM subscription WHERE node_id = ?");

	db_new_statement ("subscriptionLoadStmt",
	                  "SELECT "
			  "node_id,"
			  "source,"
			  "orig_source,"
			  "filter_cmd,"
			  "update_interval,"
			  "default_interval,"
			  "discontinued,"
			  "available "
			  "FROM subscription");

	db_new_statement ("subscriptionMetadataLoadStmt",
	                  "SELECT key,value,nr FROM subscription_metadata WHERE node_id = ? ORDER BY nr");

	db_new_statement ("subscriptionMetadataUpdateStmt",
	                  "REPLACE INTO subscription_metadata (node_id,nr,key,value) VALUES (?,?,?,?)");

	db_new_statement ("nodeUpdateStmt",
	                  "REPLACE INTO node (node_id,parent_id,title,type,expanded,view_mode,sort_column,sort_reversed) VALUES (?,?,?,?,?,0,?,?)");

	db_new_statement ("itemUpdateSearchFoldersStmt",
	                  "REPLACE INTO search_folder_items (node_id, parent_node_id, item_id) VALUES (?,?,?)");

	db_new_statement ("itemRemoveFromSearchFolderStmt",
	                  "DELETE FROM search_folder_items WHERE node_id =? AND item_id = ?;");

	db_new_statement ("searchFolderLoadStmt",
	                  "SELECT item_id FROM search_folder_items WHERE node_id = ?;");

	db_new_statement ("searchFolderCountStmt",
	                  "SELECT count(item_id) FROM search_folder_items WHERE node_id = ?;");

	db_new_statement ("searchFolderUnreadCountStmt",
	                  "SELECT count(items.item_id) FROM search_folder_items JOIN items ON search_folder_items.item_id = items.item_id WHERE search_folder_items.node_id = ? and items.read = 0;");

	db_new_statement ("nodeIdListStmt",
	                  "SELECT node_id FROM node;");

	db_new_statement ("nodeRemoveStmt",
	                  "DELETE FROM node WHERE node_id = ?;");

	g_assert (sqlite3_get_autocommit (db));

	debug_exit ("db_init");
}

void
db_deinit (void)
{

	debug_enter ("db_deinit");

	if (FALSE == sqlite3_get_autocommit (db))
		g_warning ("Fatal: DB not in auto-commit mode. This is a bug. Data may be lost!");

	if (statements) {
		g_hash_table_destroy (statements);
		statements = NULL;
	}

	if (SQLITE_OK != sqlite3_close (db))
		g_warning ("DB close failed: %s", sqlite3_errmsg (db));

	db = NULL;

	debug_exit ("db_deinit");
}

static GSList *
db_metadata_list_append (GSList *metadata, const char *key, const char *value)
{
	if (metadata_is_type_registered (key))
		metadata = metadata_list_append (metadata, key, value);
	else
		debug1 (DEBUG_DB, "Trying to load unregistered metadata type %s from DB.", key);

	return metadata;
}

static GSList *
db_item_metadata_load(itemPtr item)
{
	GSList		*metadata = NULL;
	sqlite3_stmt 	*stmt;
	gint		res;

	stmt = db_get_statement ("metadataLoadStmt");
	res = sqlite3_bind_int (stmt, 1, item->id);
	if (SQLITE_OK != res)
		g_error ("db_item_load_metadata: sqlite bind failed (error code %d)!", res);

	while (sqlite3_step (stmt) == SQLITE_ROW) {
		const char *key, *value;
		key = (const char *) sqlite3_column_text(stmt, 0);
		value = (const char *) sqlite3_column_text(stmt, 1);
		if (g_str_equal (key, "enclosure"))
			item->hasEnclosure = TRUE;
		metadata = db_metadata_list_append (metadata, key, value);
	}

	sqlite3_finalize (stmt);

	return metadata;
}

static void
db_item_metadata_update_cb (const gchar *key,
                            const gchar *value,
                            guint index,
                            gpointer user_data)
{
	sqlite3_stmt	*stmt;
	itemPtr		item = (itemPtr)user_data;
	gint		res;

	stmt = db_get_statement ("metadataUpdateStmt");
	sqlite3_bind_int  (stmt, 1, item->id);
	sqlite3_bind_int  (stmt, 2, index);
	sqlite3_bind_text (stmt, 3, key, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 4, value, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);
	if (SQLITE_DONE != res)
		g_warning ("Update in \"metadata\" table failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (stmt);

}

static void
db_item_metadata_update(itemPtr item)
{
	metadata_list_foreach(item->metadata, db_item_metadata_update_cb, item);
}

/* Item structure loading methods */

static itemPtr
db_load_item_from_columns (sqlite3_stmt *stmt)
{
	const gchar	*tmp;

	itemPtr item = item_new ();

	item->readStatus	= sqlite3_column_int (stmt, 1)?TRUE:FALSE;
	item->updateStatus	= sqlite3_column_int (stmt, 2)?TRUE:FALSE;
	item->popupStatus	= sqlite3_column_int (stmt, 3)?TRUE:FALSE;
	item->flagStatus	= sqlite3_column_int (stmt, 4)?TRUE:FALSE;
	item->validGuid		= sqlite3_column_int (stmt, 7)?TRUE:FALSE;
	item->time		= sqlite3_column_int64 (stmt, 9);
	item->commentFeedId	= g_strdup ((const gchar *) sqlite3_column_text (stmt, 10));
	item->isComment		= sqlite3_column_int (stmt, 11);
	item->id		= sqlite3_column_int (stmt, 12);
	item->parentItemId	= sqlite3_column_int (stmt, 13);
	item->nodeId		= g_strdup ((const gchar *) sqlite3_column_text (stmt, 14));
	item->parentNodeId	= g_strdup ((const gchar *) sqlite3_column_text (stmt, 15));

	item->title		= g_strdup ((const gchar *) sqlite3_column_text(stmt, 0));
	item->sourceId		= g_strdup ((const gchar *) sqlite3_column_text(stmt, 6));

	tmp = (const gchar *) sqlite3_column_text(stmt, 5);
	if (tmp)
		item->source = g_strdup (tmp);

	tmp = (const gchar *) sqlite3_column_text(stmt, 8);
	if (tmp)
		item->description = g_strdup (tmp);
	else
		item->description = g_strdup ("");

	item->metadata = db_item_metadata_load (item);

	return item;
}

itemSetPtr
db_itemset_load (const gchar *id)
{
	sqlite3_stmt	*stmt;
	itemSetPtr 	itemSet;

	debug1 (DEBUG_DB, "loading itemset for node \"%s\"", id);
	itemSet = g_new0 (struct itemSet, 1);
	itemSet->nodeId = (gchar *)id;

	stmt = db_get_statement ("itemsetLoadStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);

	while (sqlite3_step (stmt) == SQLITE_ROW) {
		itemSet->ids = g_list_append (itemSet->ids, GUINT_TO_POINTER (sqlite3_column_int (stmt, 0)));
	}

	sqlite3_finalize (stmt);

	debug0 (DEBUG_DB, "loading of itemset finished");

	return itemSet;
}

itemPtr
db_item_load (gulong id)
{
	sqlite3_stmt	*stmt;
	itemPtr 	item = NULL;

	debug1 (DEBUG_DB, "loading item %lu", id);
	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("itemLoadStmt");
	sqlite3_bind_int (stmt, 1, id);

	if (sqlite3_step (stmt) == SQLITE_ROW) {
		item = db_load_item_from_columns (stmt);
		(void) sqlite3_step (stmt);
	} else {
		debug1 (DEBUG_DB, "Could not load item with id %lu!", id);
	}

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "item load");

	return item;
}

/* Item modification methods */

static void
db_item_search_folders_update (itemPtr item)
{
	sqlite3_stmt	*stmt;
	gint 		res;
	GSList		*iter, *list;

	/* Bail on comments which are not covered by search folders */
	if (item->isComment)
		return;

	/* Add item to all search folders it now belongs to */

	stmt = db_get_statement ("itemUpdateSearchFoldersStmt");
	iter = list = vfolder_get_all_with_item_id (item);
	while (iter) {
		vfolderPtr vfolder = (vfolderPtr)iter->data;
		sqlite3_reset (stmt);
		sqlite3_bind_text (stmt, 1, vfolder->node->id, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text (stmt, 2, item->nodeId, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int (stmt, 3, item->id);
		res = sqlite3_step (stmt);

		if (SQLITE_DONE != res)
			g_warning ("item add to search folder failed (error code=%d, %s)", res, sqlite3_errmsg (db));
		iter = g_slist_next (iter);

	}
	g_slist_free (list);

	sqlite3_finalize (stmt);

	/* Remove item from all search folders it does not belong
	   (we do not check if it is in there, just remove it) */

	stmt = db_get_statement ("itemRemoveFromSearchFolderStmt");
	iter = list = vfolder_get_all_without_item_id (item);
	while (iter) {
		vfolderPtr vfolder = (vfolderPtr)iter->data;
		sqlite3_reset (stmt);
		sqlite3_bind_text (stmt, 1, vfolder->node->id, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int (stmt, 2, item->id);
		res = sqlite3_step (stmt);

		if (SQLITE_DONE != res)
			g_warning ("item remove from search folder failed (error code=%d, %s)", res, sqlite3_errmsg (db));
		iter = g_slist_next (iter);

	}
	g_slist_free (list);

	sqlite3_finalize (stmt);
}

void
db_item_update (itemPtr item)
{
	sqlite3_stmt	*stmt;
	gint		res;

	debug2 (DEBUG_DB, "update of item \"%s\" (id=%lu)", item->title, item->id);
	debug_start_measurement (DEBUG_DB);

	db_begin_transaction ();

	/* Update the item... */
	stmt = db_get_statement ("itemUpdateStmt");
	sqlite3_bind_text (stmt, 1,  item->title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 2,  item->readStatus?1:0);
	sqlite3_bind_int  (stmt, 3,  item->updateStatus?1:0);
	sqlite3_bind_int  (stmt, 4,  item->popupStatus?1:0);
	sqlite3_bind_int  (stmt, 5,  item->flagStatus?1:0);
	sqlite3_bind_text (stmt, 6,  item->source, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 7,  item->sourceId, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 8,  item->validGuid?1:0);
	sqlite3_bind_text (stmt, 9,  item->description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64  (stmt, 10, item->time);
	sqlite3_bind_text (stmt, 11, item->commentFeedId, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 12, item->isComment?1:0);
	if (item->id)
		sqlite3_bind_int  (stmt, 13, item->id);
	else
		sqlite3_bind_null (stmt, 13);
	sqlite3_bind_int  (stmt, 14, item->parentItemId);
	sqlite3_bind_text (stmt, 15, item->nodeId, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 16, item->parentNodeId, -1, SQLITE_TRANSIENT);

	res = sqlite3_step (stmt);

	if (SQLITE_DONE != res)
		g_warning ("item update failed (error code=%d, %s)", res, sqlite3_errmsg (db));
	if (!item->id && SQLITE_DONE == res) {
		item->id = sqlite3_last_insert_rowid (db);
		debug2(DEBUG_DB, "insert into table \"items\": \"%s\" id : %lu", item->title, item->id);
	}

	sqlite3_finalize (stmt);

	db_item_metadata_update (item);
	db_item_search_folders_update (item);

	db_end_transaction ();

	debug_end_measurement (DEBUG_DB, "item update");
}

void
db_item_state_update (itemPtr item)
{
	sqlite3_stmt	*stmt;

	if (!item->id) {
		db_item_update (item);
		return;
	}

	db_item_search_folders_update (item);

	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("itemStateUpdateStmt");
	sqlite3_bind_int (stmt, 1, item->readStatus?1:0);
	sqlite3_bind_int (stmt, 2, item->flagStatus?1:0);
	sqlite3_bind_int (stmt, 3, item->updateStatus?1:0);
	sqlite3_bind_int (stmt, 4, item->id);

	if (sqlite3_step (stmt) != SQLITE_DONE)
		g_warning ("item state update failed (%s)", sqlite3_errmsg (db));

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "item state update");

}

void
db_item_remove (gulong id)
{
	sqlite3_stmt	*stmt;
	gint		res;

	debug1 (DEBUG_DB, "removing item with id %lu", id);

	stmt = db_get_statement ("itemsetRemoveStmt");
	sqlite3_bind_int (stmt, 1, id);
	sqlite3_bind_int (stmt, 2, id);
	res = sqlite3_step (stmt);

	if (SQLITE_DONE != res)
		g_warning ("item remove failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (stmt);
}

GSList *
db_item_get_duplicates (const gchar *guid)
{
	GSList		*duplicates = NULL;
	sqlite3_stmt	*stmt;
	gint		res;

	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("duplicatesFindStmt");
	res = sqlite3_bind_text (stmt, 1, guid, -1, SQLITE_TRANSIENT);
	if (SQLITE_OK != res)
		g_error ("db_item_get_duplicates: sqlite bind failed (error code %d)!", res);

	while (sqlite3_step (stmt) == SQLITE_ROW)
	{
		gulong id = sqlite3_column_int (stmt, 0);
		duplicates = g_slist_append (duplicates, GUINT_TO_POINTER (id));
	}

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "searching for duplicates");

	return duplicates;
}

GSList *
db_item_get_duplicate_nodes (const gchar *guid)
{
	GSList		*duplicates = NULL;
	sqlite3_stmt	*stmt;
	gint		res;

	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("duplicateNodesFindStmt");
	res = sqlite3_bind_text (stmt, 1, guid, -1, SQLITE_TRANSIENT);
	if (SQLITE_OK != res)
		g_error ("db_item_get_duplicates: sqlite bind failed (error code %d)!", res);

	while (sqlite3_step (stmt) == SQLITE_ROW)
	{
		gchar *id = g_strdup((const gchar *) sqlite3_column_text (stmt, 0));
		duplicates = g_slist_append (duplicates, id);
	}

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "searching for duplicates");

	return duplicates;
}

void
db_itemset_remove_all (const gchar *id)
{
	sqlite3_stmt	*stmt;
	gint		res;

	debug1(DEBUG_DB, "removing all items for item set with %s", id);

	stmt = db_get_statement ("itemsetRemoveAllStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 2, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);

	if (SQLITE_DONE != res)
		g_warning ("removing all items failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (stmt);

}

void
db_itemset_mark_all_popup (const gchar *id)
{
	sqlite3_stmt	*stmt;
	gint		res;

	debug1 (DEBUG_DB, "marking all items popup for item set with %s", id);

	stmt = db_get_statement ("itemsetMarkAllPopupStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);

	if (SQLITE_DONE != res)
		g_warning ("marking all items popup failed (error code=%d, %s)", res, sqlite3_errmsg(db));

	sqlite3_finalize (stmt);

}

gboolean
db_itemset_get (itemSetPtr itemSet, gulong offset, guint limit)
{
	sqlite3_stmt	*stmt;
	gboolean	success = FALSE;

	debug2 (DEBUG_DB, "loading %d items offset %lu", limit, offset);

	stmt = db_get_statement ("itemsetLoadOffsetStmt");
	sqlite3_bind_int (stmt, 1, limit);
	sqlite3_bind_int (stmt, 2, offset);

	while (sqlite3_step (stmt) == SQLITE_ROW) {
		itemSet->ids = g_list_append (itemSet->ids, GUINT_TO_POINTER (sqlite3_column_int (stmt, 0)));
		success = TRUE;
	}

	sqlite3_finalize (stmt);

	return success;
}

/* Statistics interface */

guint
db_itemset_get_unread_count (const gchar *id)
{
	sqlite3_stmt	*stmt;
	gint		res;
	guint		count = 0;

	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("itemsetReadCountStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);

	if (SQLITE_ROW == res)
		count = sqlite3_column_int (stmt, 0);
	else
		g_warning("item read counting failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "counting unread items");

	return count;
}

guint
db_itemset_get_item_count (const gchar *id)
{
	sqlite3_stmt 	*stmt;
	gint		res;
	guint		count = 0;

	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("itemsetItemCountStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);

	if (SQLITE_ROW == res)
		count = sqlite3_column_int (stmt, 0);
	else
		g_warning ("item counting failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "counting items");

	return count;
}

/* This method is only used for migration from old schema versions */
static void
db_view_remove_triggers (const gchar *id)
{
	gchar	*sql, *err;
	gint	res;

	err = NULL;
	sql = sqlite3_mprintf ("DROP TRIGGER view_%s_insert_before;", id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		debug2 (DEBUG_DB, "Dropping trigger failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);

	err = NULL;
	sql = sqlite3_mprintf ("DROP TRIGGER view_%s_insert_after;", id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		debug2 (DEBUG_DB, "Dropping trigger failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);

	err = NULL;
	sql = sqlite3_mprintf ("DROP TRIGGER view_%s_delete;", id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		debug2 (DEBUG_DB, "Dropping trigger failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);

	err = NULL;
	sql = sqlite3_mprintf ("DROP TRIGGER view_%s_update_before;", id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		debug2 (DEBUG_DB, "Dropping trigger failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);

	err = NULL;
	sql = sqlite3_mprintf ("DROP TRIGGER view_%s_update_after;", id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		debug2 (DEBUG_DB, "Dropping trigger failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);
}

/* This method is only used for migration from old schema versions */
static void
db_view_remove (const gchar *id)
{
	gchar	*sql, *err;
	gint	res;

	debug1 (DEBUG_DB, "Dropping view \"%s\"", id);

	db_view_remove_triggers (id);

	/* Note: no need to remove anything from view_state, as this
	   is dropped on schema migration and this method is only
	   used during schema migration to remove all views. */

	sql = sqlite3_mprintf ("DROP VIEW view_%s;", id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_warning ("Dropping view failed (%s) SQL: %s", err, sql);

	sqlite3_free (sql);
	sqlite3_free (err);
}

itemSetPtr
db_search_folder_load (const gchar *id)
{
	gint		res;
	sqlite3_stmt	*stmt;
	itemSetPtr 	itemSet;

	debug1 (DEBUG_DB, "loading search folder node \"%s\"", id);

	stmt = db_get_statement ("searchFolderLoadStmt");
	res = sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	if (SQLITE_OK != res)
		g_error ("db_search_folder_load: sqlite bind failed (error code %d)!", res);

	itemSet = g_new0 (struct itemSet, 1);
	itemSet->nodeId = (gchar *)id;

	while (sqlite3_step (stmt) == SQLITE_ROW) {
		itemSet->ids = g_list_append (itemSet->ids, GUINT_TO_POINTER (sqlite3_column_int (stmt, 0)));
	}

	sqlite3_finalize (stmt);

	debug1 (DEBUG_DB, "loading search folder finished (%d items)", g_list_length (itemSet->ids));

	return itemSet;
}

void
db_search_folder_reset (const gchar *id)
{
	gchar	*sql, *err;
	gint	res;

	debug1 (DEBUG_DB, "resetting search folder node \"%s\"", id);

	sql = sqlite3_mprintf ("DELETE FROM search_folder_items WHERE node_id = '%s';", id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_warning ("resetting search folder failed (%s) SQL: %s", err, sql);

	sqlite3_free (sql);
	sqlite3_free (err);

	debug0 (DEBUG_DB, "removing search folder finished");
}

void
db_search_folder_add_items (const gchar *id, GSList *items)
{
	sqlite3_stmt	*stmt;
	GSList		*iter;
	gint	res;

	debug2 (DEBUG_DB, "add %d items to search folder node \"%s\"", g_slist_length (items), id);

	stmt = db_get_statement ("itemUpdateSearchFoldersStmt");

	iter = items;
	while (iter) {
		itemPtr item = (itemPtr)iter->data;

		sqlite3_reset (stmt);
		sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text (stmt, 2, item->nodeId, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int (stmt, 3, item->id);
		res = sqlite3_step (stmt);
		if (SQLITE_DONE != res)
			g_error ("db_search_folder_add_items: sqlite3_step (error code %d)!", res);

		iter = g_slist_next (iter);

	}

	sqlite3_finalize (stmt);

	debug0 (DEBUG_DB, "adding items to search folder finished");
}

guint
db_search_folder_get_item_count (const gchar *id)
{
	sqlite3_stmt	*stmt;
	gint		res;
	guint		count = 0;

	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("searchFolderCountStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);

	if (SQLITE_ROW == res)
		count = sqlite3_column_int (stmt, 0);
	else
		g_warning("item read counting failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "counting unread items");

	return count;
}

guint
db_search_folder_get_unread_count (const gchar *id)
{
	sqlite3_stmt	*stmt;
	gint		res;
	guint		count = 0;

	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("searchFolderUnreadCountStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);

	if (SQLITE_ROW == res)
		count = sqlite3_column_int (stmt, 0);
	else
		g_warning("item unread counting failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "counting unread items");

	return count;
}

static GSList *
db_subscription_metadata_load (const gchar *id)
{
	GSList		*metadata = NULL;
	sqlite3_stmt	*stmt;
	gint		res;

	stmt = db_get_statement ("subscriptionMetadataLoadStmt");
	res = sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	if (SQLITE_OK != res)
		g_error ("db_subscription_metadata_load: sqlite bind failed (error code %d)!", res);

	while (sqlite3_step (stmt) == SQLITE_ROW) {
		metadata = db_metadata_list_append (metadata, (const char *) sqlite3_column_text(stmt, 0),
		                                           (const char *) sqlite3_column_text(stmt, 1));
	}

	sqlite3_finalize (stmt);

	return metadata;
}

static void
db_subscription_metadata_update_cb (const gchar *key,
                                    const gchar *value,
                                    guint index,
                                    gpointer user_data)
{
	sqlite3_stmt	*stmt;
	nodePtr		node = (nodePtr)user_data;
	gint		res;

	stmt = db_get_statement ("subscriptionMetadataUpdateStmt");
	sqlite3_bind_text (stmt, 1, node->id, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 2, index);
	sqlite3_bind_text (stmt, 3, key, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 4, value, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);
	if (SQLITE_DONE != res)
		g_warning ("Update in \"subscription_metadata\" table failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (stmt);
}

static void
db_subscription_metadata_update (subscriptionPtr subscription)
{
	metadata_list_foreach (subscription->metadata, db_subscription_metadata_update_cb, subscription->node);
}

void
db_subscription_load (subscriptionPtr subscription)
{
	subscription->metadata = db_subscription_metadata_load (subscription->node->id);
}

void
db_subscription_update (subscriptionPtr subscription)
{
	sqlite3_stmt	*stmt;
	gint		res;

	debug1 (DEBUG_DB, "updating subscription info %s", subscription->node->id);
	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("subscriptionUpdateStmt");
	sqlite3_bind_text (stmt, 1, subscription->node->id, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 2, subscription->source, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 3, subscription->origSource, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 4, subscription->filtercmd, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 5, subscription->updateInterval);
	sqlite3_bind_int  (stmt, 6, subscription->defaultInterval);
	sqlite3_bind_int  (stmt, 7, subscription->discontinued?1:0);
	sqlite3_bind_int  (stmt, 8, (subscription->updateError ||
	                             subscription->httpError ||
				     subscription->filterError)?1:0);

	res = sqlite3_step (stmt);
	if (SQLITE_DONE != res)
		g_warning ("Could not update subscription info for node id %s in DB (error code %d)!", subscription->node->id, res);

	sqlite3_finalize (stmt);

	db_subscription_metadata_update (subscription);

	debug_end_measurement (DEBUG_DB, "subscription update");
}

void
db_subscription_remove (const gchar *id)
{
	sqlite3_stmt	*stmt;
	gint		res;

	debug1 (DEBUG_DB, "removing subscription %s", id);
	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("subscriptionRemoveStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);

	res = sqlite3_step (stmt);
	if (SQLITE_DONE != res)
		g_warning ("Could not remove subscription %s from DB (error code %d)!", id, res);

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "subscription remove");
}

void
db_node_update (nodePtr node)
{
	sqlite3_stmt	*stmt;
	gint		res;

	debug1 (DEBUG_DB, "updating node info %s", node->id);
	debug_start_measurement (DEBUG_DB);

	stmt = db_get_statement ("nodeUpdateStmt");
	sqlite3_bind_text (stmt, 1, node->id, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 2, node->parent->id, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 3, node->title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 4, node_type_to_str (node), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 5, node->expanded?1:0);
	sqlite3_bind_int  (stmt, 6, node->sortColumn);
	sqlite3_bind_int  (stmt, 7, node->sortReversed?1:0);

	res = sqlite3_step (stmt);
	if (SQLITE_DONE != res)
		g_warning ("Could not update node info %s in DB (error code %d)!", node->id, res);

	sqlite3_finalize (stmt);

	debug_end_measurement (DEBUG_DB, "node update");
}

static gboolean
db_node_find (nodePtr node, gpointer id)
{
	GSList *iter;

	if (g_str_equal (node->id, (gchar *)id))
		return TRUE;

	iter = node->children;
	while (iter) {
		if (db_node_find ((nodePtr)iter->data, id))
			return TRUE;
		iter = g_slist_next (iter);
	}

	return FALSE;
}

static void
db_node_remove (const gchar *id)
{
	sqlite3_stmt	*stmt;
	gint		res;

	stmt = db_get_statement ("nodeRemoveStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);

	res = sqlite3_step (stmt);
	if (SQLITE_DONE != res)
		g_warning ("Could not remove node %s in DB (error code %d)!", id, res);

	sqlite3_finalize (stmt);
}

void
db_node_cleanup (nodePtr root)
{
	sqlite3_stmt	*stmt;

	debug0 (DEBUG_DB, "Cleaning node ids...");

	/* Fetch all node ids */
	stmt = db_get_statement ("nodeIdListStmt");
	while (sqlite3_step (stmt) == SQLITE_ROW) {
		/* Drop node ids not in feed list anymore */
		const gchar *id = (const gchar *) sqlite3_column_text (stmt, 0);
		if (id && !db_node_find (root, (gpointer)id)) {
			db_subscription_remove (id);	/* in case it is a subscription */
			db_node_remove (id);		/* in case it is a folder */
		}
	}

	sqlite3_finalize (stmt);
}
