/**
 * @file db.c sqlite backend
 * 
 * Copyright (C) 2007  Lars Lindner <lars.lindner@gmail.com>
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
#include "db.h"
#include "debug.h"
#include "item.h"
#include "itemset.h"
#include "metadata.h"

static sqlite3	*db = NULL;
static guint	stmtCounter = 0;

/** 
 * To avoid loosing statements on crashes, close+reopen the DB from time to time,
 * THIS IS A WORKAROUND TO AVOID A STRANGE EFFECT OF LOOSING ALL TRANSACTIONS ON EXIT. 
 */
#define MAX_STATEMENTS_BEFORE_RECONNECT	500

/** value structure of statements hash */ 
typedef struct statement {
	sqlite3_stmt	*stmt;	/** the prepared statement */
	gboolean	write;	/** TRUE if statement modifies DB and should be counted to stmtCounter */
} statement;

/** hash of all prepared statements */
static GHashTable *statements = NULL;

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
	struct statement *statement;
	
	statement = g_new0 (struct statement, 1);
	if (strstr (sql, "INSERT") || strstr (sql, "REPLACE"))
		statement->write = TRUE;
	db_prepare_stmt (&statement->stmt, sql);
	
	if (!statements)
		statements = g_hash_table_new (g_str_hash, g_str_equal);
				
	g_hash_table_insert (statements, (gpointer)name, (gpointer)statement);
}

static sqlite3_stmt *
db_get_statement (const gchar *name)
{
	struct statement *statement;

redo:
	statement = (struct statement *) g_hash_table_lookup (statements, name);
	if (!statement)
		g_error ("Fatal: unknown prepared statement \"%s\" requested!", name);	
		
	if (statement->write)
		stmtCounter++;
		
	if (stmtCounter > MAX_STATEMENTS_BEFORE_RECONNECT) {
		stmtCounter = 0;
		debug1 (DEBUG_DB, "DB reconnect after %d DB write actions...\n", MAX_STATEMENTS_BEFORE_RECONNECT); 
		db_deinit ();
		db_init (FALSE);
		goto redo;
	}

	sqlite3_reset (statement->stmt);
	return statement->stmt;
}

static void
db_exec (const gchar *sql)
{
	gchar	*err;
	gint	res;
	
	debug1 (DEBUG_DB, "executing SQL: %s", sql);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	debug2 (DEBUG_DB, " -> result: %d (%s)", res, err?err:"success");	
	sqlite3_free (err);
}

static gboolean
db_table_exists (const gchar *name)
{
	gchar		*sql;
	sqlite3_stmt	*stmt;
	gint		res;

	sql = sqlite3_mprintf ("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = '%s';", name);
	db_prepare_stmt (&stmt, sql);
	sqlite3_reset (stmt);
	sqlite3_step (stmt);
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
	guint		schemaVersion;
	sqlite3_stmt	*stmt;
	
	if (!db_table_exists ("info")) {
		db_exec ("CREATE TABLE info ( "
		         "   name	TEXT, "
			 "   value	TEXT, "
		         "   PRIMARY KEY (name) "
		         ");");
		return -1;
	}
	
	db_prepare_stmt (&stmt, "SELECT value FROM info WHERE name = 'schemaVersion'");
	sqlite3_step (stmt);
	schemaVersion = sqlite3_column_int (stmt, 0);
	sqlite3_finalize (stmt);
	
	return schemaVersion;
}

#define SCHEMA_TARGET_VERSION 6
	
/* opening or creation of database */
void
db_init (gboolean initial) 
{
	gchar		*filename;
	gint		schemaVersion;
		
	debug_enter ("db_init");
	
	stmtCounter = 0;

open:
	filename = common_create_cache_filename (NULL, "liferea", "db");
	debug1 (DEBUG_DB, "Opening DB file %s...", filename);
	if (!sqlite3_open (filename, &db)) {
		debug1 (DEBUG_CACHE, "Data base file %s was not found... Creating new one.", filename);
	}
	g_free (filename);
	
	sqlite3_extended_result_codes (db, TRUE);
	
	if (initial) {
		/* create info table/check versioning info */				   
		schemaVersion = db_get_schema_version ();
		debug1 (DEBUG_DB, "current DB schema version: %d", schemaVersion);

		if (-1 == schemaVersion) {
			/* no schema version available -> first installation without tables... */
			db_set_schema_version (SCHEMA_TARGET_VERSION);
			schemaVersion = SCHEMA_TARGET_VERSION;	/* nothing exists yet, tables will be created below */
		}

		if (SCHEMA_TARGET_VERSION < schemaVersion)
			g_error ("Fatal: The cache database was created by a newer version of Liferea than this one!");

		if (SCHEMA_TARGET_VERSION > schemaVersion) {
			/* do table migration */	
			if (db_get_schema_version () == 0) {
				/* 1.3.2 -> 1.3.3 adding read flag to itemsets relation */
				debug0 (DEBUG_DB, "migrating from schema version 0 to 1");
				db_exec ("BEGIN; "
	        			 "CREATE TEMPORARY TABLE itemsets_backup(item_id,node_id); "
					 "INSERT INTO itemsets_backup SELECT item_id,node_id FROM itemsets; "
	        			 "DROP TABLE itemsets; "
                       			 "CREATE TABLE itemsets ( "
					 "	item_id		INTEGER, "
					 "	node_id		TEXT, "
					 "	read		INTEGER "
	        			 "); "
					 "INSERT INTO itemsets SELECT itemsets_backup.item_id,itemsets_backup.node_id,items.read FROM itemsets_backup INNER JOIN items ON itemsets_backup.item_id = items.ROWID; "
					 "DROP TABLE itemsets_backup; "
	        			 "REPLACE INTO info (name, value) VALUES ('schemaVersion',1); "
					 "END;");
			}

			if (db_get_schema_version () == 1) {
				/* 1.3.3 -> 1.3.4 adding comment item flag to itemsets relation */
				debug0 (DEBUG_DB, "migrating from schema version 1 to 2");
				db_exec ("BEGIN; "
	        			 "CREATE TEMPORARY TABLE itemsets_backup(item_id,node_id,read); "
					 "INSERT INTO itemsets_backup SELECT item_id,node_id,read FROM itemsets; "
	        			 "DROP TABLE itemsets; "
                       			 "CREATE TABLE itemsets ( "
					 "	item_id		INTEGER, "
					 "	node_id		TEXT, "
					 "	read		INTEGER, "
					 "      comment		INTEGER "
	        			 "); "
					 "INSERT INTO itemsets SELECT itemsets_backup.item_id,itemsets_backup.node_id,itemsets_backup.read,0 FROM itemsets_backup; "
					 "DROP TABLE itemsets_backup; "
					 "CREATE TEMPORARY TABLE items_backup(title,read,new,updated,popup,marked,source,source_id,valid_guid,real_source_url,real_source_title,description,date,comment_feed_id);"
					 "INSERT INTO items_backup SELECT title,read,new,updated,popup,marked,source,source_id,valid_guid,real_source_url,real_source_title,description,date,comment_feed_id FROM items; "
					 "DROP TABLE items; "
					 "CREATE TABLE items ("
			        	 "   title		TEXT,"
			        	 "   read		INTEGER,"
			        	 "   new		INTEGER,"
			        	 "   updated		INTEGER,"
			        	 "   popup		INTEGER,"
			        	 "   marked		INTEGER,"
			        	 "   source		TEXT,"
			        	 "   source_id		TEXT,"
			        	 "   valid_guid		INTEGER,"
			        	 "   real_source_url	TEXT,"
			        	 "   real_source_title	TEXT,"
			        	 "   description	TEXT,"
			        	 "   date		INTEGER,"
			        	 "   comment_feed_id	INTEGER,"
					 "   comment            INTEGER"
			        	 "); "
					 "INSERT INTO items SELECT title,read,new,updated,popup,marked,source,source_id,valid_guid,real_source_url,real_source_title,description,date,comment_feed_id,0 FROM items_backup; "
					 "DROP TABLE items_backup; "
	        			 "REPLACE INTO info (name, value) VALUES ('schemaVersion',2); "
					 "END;");
			}

			if (db_get_schema_version () == 2) {
				/* 1.3.5 -> 1.3.6 adding subscription relation */
				debug0 (DEBUG_DB, "migrating from schema version 2 to 3");
				db_exec ("BEGIN; "
			        	 "CREATE TABLE SUBSCRIPTION ("
		                	 "   NODE_ID            STRING,"
		                	 "   PRIMARY KEY (NODE_ID)"
			        	 "); "
					 "INSERT INTO subscription SELECT DISTINCT node_id FROM itemsets; "
					 "REPLACE INTO info (name, value) VALUES ('schemaVersion',3); "
					 "END;");
			}

			if (db_get_schema_version () == 3) {
				/* 1.3.6 -> 1.3.7 adding all necessary attributes to subscription relation */
				debug0 (DEBUG_DB, "migrating from schema version 3 to 4");
				db_exec ("BEGIN; "
			        	 "CREATE TEMPORARY TABLE subscription_backup(node_id); "
					 "INSERT INTO subscription_backup SELECT node_id FROM subscription; "
			        	 "DROP TABLE subscription; "
			        	 "CREATE TABLE subscription ("
		                	 "   node_id            STRING,"
					 "   source             STRING,"
					 "   orig_source        STRING,"
					 "   filter_cmd         STRING,"
					 "   update_interval	INTEGER,"
					 "   default_interval   INTEGER,"
					 "   discontinued       INTEGER,"
					 "   available          INTEGER,"
		                	 "   PRIMARY KEY (node_id)"
			        	 "); "
					 "INSERT INTO subscription SELECT node_id,null,null,null,0,0,0,0 FROM subscription_backup; "
					 "DROP TABLE subscription_backup; "
					 "REPLACE INTO info (name, value) VALUES ('schemaVersion',4); "
					 "END;");
			}

			if (db_get_schema_version () == 4) {
				/* 1.3.8 -> 1.4-RC1 adding node relation */
				debug0 (DEBUG_DB, "migrating from schema version 4 to 5");
				/* table created below... */
				db_set_schema_version (5);
			}
			
			if (db_get_schema_version () == 5) {
				/* 1.4.9 -> 1.4.10 adding parent_item_id to itemset relation */
				debug0 (DEBUG_DB, "migrating from schema version 5 to 6 (this drops all comments)");
				db_exec ("BEGIN; "
				         "DELETE FROM itemsets WHERE comment = 1; "
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

			if (SCHEMA_TARGET_VERSION != db_get_schema_version ())
				g_error ("Fatal: DB schema migration failed! Running with --debug-db could give some hints!");

			db_deinit ();			
			debug0 (DEBUG_DB, "Reopening DB after migration...");
			goto open;
		}

		debug_start_measurement (DEBUG_DB);
		db_begin_transaction ();

		/* create tables if they do not exist yet */
		db_exec ("CREATE TABLE items ("
	        	 "   title		TEXT,"
	        	 "   read		INTEGER,"
	        	 "   new		INTEGER,"
	        	 "   updated		INTEGER,"
	        	 "   popup		INTEGER,"
	        	 "   marked		INTEGER,"
	        	 "   source		TEXT,"
	        	 "   source_id		TEXT,"
	        	 "   valid_guid		INTEGER,"
	        	 "   real_source_url	TEXT,"
	        	 "   real_source_title	TEXT,"
	        	 "   description	TEXT,"
	        	 "   date		INTEGER,"
	        	 "   comment_feed_id	TEXT,"
			 "   comment            INTEGER"
	        	 ");");

		db_exec ("CREATE INDEX items_idx ON items (source_id);");
		db_exec ("CREATE INDEX items_idx2 ON items (comment_feed_id);");

		db_exec ("CREATE TABLE itemsets ("
	        	 "   item_id		INTEGER,"
			 "   parent_item_id     INTEGER,"
	        	 "   node_id		TEXT,"
	        	 "   read		INTEGER,"
			 "   comment            INTEGER,"
	        	 "   PRIMARY KEY (item_id, node_id)"
	        	 ");");

		db_exec ("CREATE INDEX itemset_idx  ON itemsets (node_id);");
		db_exec ("CREATE INDEX itemset_idx2 ON itemsets (item_id);");
		
		db_exec ("CREATE TABLE metadata ("
	        	 "   item_id		INTEGER,"
	        	 "   nr              	INTEGER,"
	        	 "   key             	TEXT,"
	        	 "   value           	TEXT,"
	        	 "   PRIMARY KEY (item_id, nr)"
	        	 ");");

		db_exec ("CREATE INDEX metadata_idx ON metadata (item_id);");

		/* Set up item removal trigger */	
		db_exec ("DROP TRIGGER item_removal;");
		db_exec ("CREATE TRIGGER item_removal DELETE ON itemsets "
	        	 "BEGIN "
	        	 "   DELETE FROM items WHERE ROWID = old.item_id; "
			 "   DELETE FROM itemsets WHERE parent_item_id = old.item_id; "
			 "   DELETE FROM metadata WHERE item_id = old.item_id; "
	        	 "END;");

		/* Set up item read state update triggers */
		db_exec ("DROP TRIGGER item_insert;");
		db_exec ("CREATE TRIGGER item_insert INSERT ON items "
	        	 "BEGIN "
	        	 "   UPDATE itemsets SET read = new.read "
	        	 "   WHERE item_id = new.ROWID; "
	        	 "END;");

		db_exec ("DROP TRIGGER item_update;");
		db_exec ("CREATE TRIGGER item_update UPDATE ON items "
	        	 "BEGIN "
	        	 "   UPDATE itemsets SET read = new.read "
	        	 "   WHERE item_id = new.ROWID; "
	        	 "END;");

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

		db_exec ("CREATE TABLE update_state ("
	        	 "   node_id            STRING,"
			 "   last_modified      STRING,"
			 "   etag               STRING,"
			 "   last_update        INTEGER,"
			 "   last_favicon_update INTEGER,"
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

		/* Set up subscription removal trigger */	
		db_exec ("DROP TRIGGER subscription_removal;");
		db_exec ("CREATE TRIGGER subscription_removal DELETE ON subscription "
	        	 "BEGIN "
			 "   DELETE FROM node WHERE node_id = old.node_id; "
	        	 "   DELETE FROM update_state WHERE node_id = old.node_id; "
			 "   DELETE FROM subscription_metadata WHERE node_id = old.node_id; "
			 "   DELETE FROM itemsets WHERE node_id = old.node_id; "
	        	 "END;");

		db_exec ("CREATE TABLE node ("
	        	 "   node_id		STRING,"
	        	 "   parent_id		STRING,"
	        	 "   title		STRING,"
			 "   type		INTEGER,"
			 "   expanded           INTEGER,"
			 "   view_mode		INTEGER,"
			 "   sort_column	INTEGER,"
			 "   sort_reversed	INTEGER,"
			 "   PRIMARY KEY (node_id)"
	        	 ");");

		db_exec ("CREATE TABLE view_state ("
		         "   node_id            STRING,"
			 "   unread             INTEGER,"
			 "   count              INTEGER,"
			 "   PRIMARY KEY (node_id)"
			 ");");
			 
		/* view counting triggers are set up in the view preparation code (see db_view_create()) */

		db_end_transaction ();
		debug_end_measurement (DEBUG_DB, "table setup");

		/* Cleanup of DB */
	
		/*
		debug_start_measurement (DEBUG_DB);
		db_exec ("DELETE FROM items WHERE ROWID NOT IN "
			 "(SELECT item_id FROM itemsets);");
		debug_end_measurement (DEBUG_DB, "cleanup lost items");

		debug_start_measurement (DEBUG_DB);
		db_exec ("DELETE FROM itemsets WHERE item_id NOT IN "
			 "(SELECT ROWID FROM items);");
		debug_end_measurement (DEBUG_DB, "cleanup lost itemset entries");

		debug_start_measurement (DEBUG_DB);
		db_exec ("DELETE FROM itemsets WHERE comment = 0 AND node_id NOT IN "
	        	 "(SELECT node_id FROM subscription);");
		debug_end_measurement (DEBUG_DB, "cleanup lost node entries");
		*/
	}
	
	/* prepare statements */
	
	db_new_statement ("itemsetLoadStmt",
	                  "SELECT item_id FROM itemsets WHERE node_id = ?");
		       
	db_new_statement ("itemsetInsertStmt",
	                  "INSERT INTO itemsets ("
			  "item_id,"
			  "parent_item_id,"
			  "node_id,"
			  "read,"
			  "comment"
			  ") VALUES (?,?,?,?,?)");
	
	db_new_statement ("itemsetReadCountStmt",
	                  "SELECT COUNT(*) FROM itemsets "
		          "WHERE read = 0 AND node_id = ?");
	       
	db_new_statement ("itemsetItemCountStmt",
	                  "SELECT COUNT(*) FROM itemsets "
		          "WHERE node_id = ?");
		       
	db_new_statement ("itemsetRemoveStmt",
	                  "DELETE FROM itemsets WHERE item_id = ?");
			
	db_new_statement ("itemsetRemoveAllStmt",
	                  "DELETE FROM itemsets WHERE node_id = ?");

	db_new_statement ("itemsetMarkAllUpdatedStmt",
	                  "UPDATE items SET updated = 0 WHERE ROWID IN "
			  "(SELECT item_id FROM itemsets WHERE node_id = ?)");
			
	db_new_statement ("itemsetMarkAllOldStmt",
	                  "UPDATE items SET new = 0 WHERE ROWID IN "
			  "(SELECT item_id FROM itemsets WHERE node_id = ?)");

	db_new_statement ("itemsetMarkAllPopupStmt",
	                  "UPDATE items SET popup = 0 WHERE ROWID IN "
			  "(SELECT item_id FROM itemsets WHERE node_id = ?)");		

	db_new_statement ("itemLoadStmt",
	                  "SELECT "
	                  "items.title,"
	                  "items.read,"
	                  "items.new,"
	                  "items.updated,"
	                  "items.popup,"
	                  "items.marked,"
	                  "items.source,"
	                  "items.source_id,"
	                  "items.valid_guid,"
	                  "items.real_source_url,"
	                  "items.real_source_title,"
	                  "items.description,"
	                  "items.date,"
		          "items.comment_feed_id,"
		          "items.comment,"
		          "itemsets.item_id,"
			  "itemsets.parent_item_id, "
		          "itemsets.node_id"
	                  " FROM items INNER JOIN itemsets "
	                  "ON items.ROWID = itemsets.item_id "
	                  "WHERE items.ROWID = ?");      
	
	db_new_statement ("itemUpdateStmt",
	                  "REPLACE INTO items ("
	                  "title,"
	                  "read,"
	                  "new,"
	                  "updated,"
	                  "popup,"
	                  "marked,"
	                  "source,"
	                  "source_id,"
	                  "valid_guid,"
	                  "real_source_url,"
	                  "real_source_title,"
	                  "description,"
	                  "date,"
		          "comment_feed_id,"
		          "comment,"
	                  "ROWID"
	                  ") values (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
			
	db_new_statement ("itemMarkReadStmt",
	                  "UPDATE items SET read = 1 WHERE ROWID = ?");
					
	db_new_statement ("duplicatesFindStmt",
	                  "SELECT ROWID FROM items WHERE source_id = ?");
			 
	db_new_statement ("duplicateNodesFindStmt",
	                  "SELECT itemsets.node_id FROM itemsets WHERE itemsets.item_id IN "
			  "(SELECT items.ROWID FROM items WHERE items.source_id = ?)");
		       
	db_new_statement ("duplicatesMarkReadStmt",
 	                  "UPDATE items SET read = 1 WHERE source_id = ?");
						
	db_new_statement ("metadataLoadStmt",
	                  "SELECT key,value,nr FROM metadata WHERE item_id = ? ORDER BY nr");
			
	db_new_statement ("metadataUpdateStmt",
	                  "REPLACE INTO metadata (item_id,nr,key,value) VALUES (?,?,?,?)");
			
	db_new_statement ("updateStateLoadStmt",
	                  "SELECT "
	                  "last_modified,"
	                  "etag,"
	                  "last_update,"
	                  "last_favicon_update "
	                  "FROM update_state "
			  "WHERE node_id = ?");
			 
	db_new_statement ("updateStateSaveStmt",
	                  "REPLACE INTO update_state "
			  "(node_id,last_modified,etag,last_update,last_favicon_update) "
			  "VALUES (?,?,?,?,?)");
			 
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
			 
	db_new_statement ("subscriptionListLoadStmt",
	                  "SELECT node_id FROM subscription");
			 
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
	                  "REPLACE INTO node (node_id,parent_id,title,type,expanded,view_mode,sort_column,sort_reversed) VALUES (?,?,?,?,?,?,?,?)");

	g_assert (sqlite3_get_autocommit (db));
	
	debug_exit ("db_init");
}

static void
db_free_statements (gpointer key, gpointer value, gpointer user_data)
{
	sqlite3_finalize (((struct statement *)value)->stmt);
	g_free (value);
}

void
db_deinit (void) 
{

	debug_enter ("db_deinit");
	
	if (FALSE == sqlite3_get_autocommit(db))
		g_warning ("Fatal: DB not in auto-commit mode. This is a bug. Data may be lost!");
	
	if (statements) {
		g_hash_table_foreach (statements, db_free_statements, NULL);
		g_hash_table_destroy (statements);	
		statements = NULL;
	}
		
	if (SQLITE_OK != sqlite3_close (db))
		g_warning ("DB close failed: %s", sqlite3_errmsg (db));
	
	db = NULL;
	
	debug_exit ("db_deinit");
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
		key = sqlite3_column_text(stmt, 0);
		value = sqlite3_column_text(stmt, 1);
		if (g_str_equal (key, "enclosure"))
			item->hasEnclosure = TRUE;
		metadata = metadata_list_append (metadata, key, value); 
	}

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
	itemPtr item = item_new();
	
	item->readStatus	= sqlite3_column_int(stmt, 1)?TRUE:FALSE;
	item->newStatus		= sqlite3_column_int(stmt, 2)?TRUE:FALSE;
	item->updateStatus	= sqlite3_column_int(stmt, 3)?TRUE:FALSE;
	item->popupStatus	= sqlite3_column_int(stmt, 4)?TRUE:FALSE;
	item->flagStatus	= sqlite3_column_int(stmt, 5)?TRUE:FALSE;
	item->validGuid		= sqlite3_column_int(stmt, 8)?TRUE:FALSE;
	item->time		= sqlite3_column_int(stmt, 12);
	item->commentFeedId	= g_strdup(sqlite3_column_text(stmt, 13));
	item->isComment		= sqlite3_column_int(stmt, 14);
	item->id		= sqlite3_column_int(stmt, 15);
	item->parentItemId	= sqlite3_column_int(stmt, 16);
	item->nodeId		= g_strdup(sqlite3_column_text(stmt, 17));

	item_set_title			(item, sqlite3_column_text(stmt, 0));
	item_set_source			(item, sqlite3_column_text(stmt, 6));
	item_set_id			(item, sqlite3_column_text(stmt, 7));	
	item_set_real_source_url	(item, sqlite3_column_text(stmt, 9));
	item_set_real_source_title	(item, sqlite3_column_text(stmt, 10));
	item_set_description		(item, sqlite3_column_text(stmt, 11));

	item->metadata = db_item_metadata_load(item);

	return item;
}

itemSetPtr
db_itemset_load (const gchar *id) 
{
	sqlite3_stmt	*stmt;
	itemSetPtr 	itemSet;
	gint		res;

	debug2(DEBUG_DB, "loading itemset for node \"%s\" (thread=%p)", id, g_thread_self());
	itemSet = g_new0(struct itemSet, 1);
	itemSet->nodeId = (gchar *)id;

	stmt = db_get_statement ("itemsetLoadStmt");
	res = sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	if (SQLITE_OK != res)
		g_error ("db_itemset_load: sqlite bind failed (error code %d)!", res);

	while (sqlite3_step (stmt) == SQLITE_ROW) {
		itemSet->ids = g_list_append (itemSet->ids, GUINT_TO_POINTER (sqlite3_column_int (stmt, 0)));
	}

	debug0 (DEBUG_DB, "loading of itemset finished");
	
	return itemSet;
}

itemPtr
db_item_load (gulong id) 
{
	sqlite3_stmt	*stmt;
	itemPtr 	item = NULL;
	gint		res;

	debug2 (DEBUG_DB, "loading item %lu (thread=%p)", id, g_thread_self ());
	debug_start_measurement (DEBUG_DB);
	
	stmt = db_get_statement ("itemLoadStmt");
	res = sqlite3_bind_int (stmt, 1, id);
	if (SQLITE_OK != res)
		g_error ("db_item_load: sqlite bind failed (error code %d)!", res);

	if (sqlite3_step (stmt) == SQLITE_ROW) {
		item = db_load_item_from_columns (stmt);
		res = sqlite3_step (stmt);
		/* FIXME: sometimes (after updates) we get an unexpected SQLITE_ROW here! 
		  if(SQLITE_DONE != res)
			g_warning("Unexpected result when retrieving single item id=%lu! (error code=%d, %s)", id, res, sqlite3_errmsg(db));
		 */
	} else {
		debug2 (DEBUG_DB, "Could not load item with id #%lu (error code %d)!", id, res);
	}

	debug_end_measurement (DEBUG_DB, "item load");

	return item;
}

/* Item modification methods */

static int
db_item_set_id_cb (void *user_data,
                   int count,
		   char **values,
		   char **columns) 
{
	itemPtr	item = (itemPtr)user_data;
	
	g_assert(NULL != values);

	if(values[0]) {
		/* the result in *values should be MAX(ROWID),
		   so adding one should give a unique new id */
		item->id = 1 + atol(values[0]); 
	} else {
		/* empty table causes no result in values[0]... */
		item->id = 1;
	}
	
	debug2(DEBUG_DB, "new item id=%lu for \"%s\"", item->id, item->title);
	return 0;
}

static void
db_item_set_id (itemPtr item) 
{
	gchar	*sql, *err;
	gint	res;
	
	g_assert (0 == item->id);
	
	sql = sqlite3_mprintf ("SELECT MAX(ROWID) FROM items");
	res = sqlite3_exec (db, sql, db_item_set_id_cb, item, &err);
	if (SQLITE_OK != res) 
		g_warning ("Select failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);
}

void
db_item_update (itemPtr item) 
{
	sqlite3_stmt	*stmt;
	gint		res;
	
	debug3 (DEBUG_DB, "update of item \"%s\" (id=%lu, thread=%p)", item->title, item->id, g_thread_self());
	debug_start_measurement (DEBUG_DB);

	if(!item->id) {
		db_item_set_id(item);

		debug1(DEBUG_DB, "insert into table \"itemsets\": \"%s\"", item->title);	
		
		/* insert item <-> node relation */
		stmt = db_get_statement ("itemsetInsertStmt");
		sqlite3_bind_int  (stmt, 1, item->id);
		sqlite3_bind_int  (stmt, 2, item->parentItemId);
		sqlite3_bind_text (stmt, 3, item->nodeId, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int  (stmt, 4, item->readStatus);
		sqlite3_bind_int  (stmt, 5, item->isComment?1:0);
		res = sqlite3_step (stmt);
		if (SQLITE_DONE != res) 
			g_warning ("Insert in \"itemsets\" table failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	}

	/* Update the item... */
	stmt = db_get_statement ("itemUpdateStmt");
	sqlite3_bind_text (stmt, 1,  item->title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 2,  item->readStatus?1:0);
	sqlite3_bind_int  (stmt, 3,  item->newStatus?1:0);
	sqlite3_bind_int  (stmt, 4,  item->updateStatus?1:0);
	sqlite3_bind_int  (stmt, 5,  item->popupStatus?1:0);
	sqlite3_bind_int  (stmt, 6,  item->flagStatus?1:0);
	sqlite3_bind_text (stmt, 7,  item->source, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 8,  item->sourceId, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 9,  item->validGuid?1:0);
	sqlite3_bind_text (stmt, 10, item->real_source_url, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 11, item->real_source_title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 12, item->description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 13, item->time);
	sqlite3_bind_text (stmt, 14, item->commentFeedId, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 15, item->isComment?1:0);
	sqlite3_bind_int  (stmt, 16, item->id);
	res = sqlite3_step (stmt);

	if (SQLITE_DONE != res) 
		g_warning ("item update failed (error code=%d, %s)", res, sqlite3_errmsg (db));
	
	db_item_metadata_update (item);
	
	debug_end_measurement (DEBUG_DB, "item update");
}

void
db_item_remove (gulong id) 
{
	sqlite3_stmt	*stmt;
	gint		res;
	
	debug1 (DEBUG_DB, "removing item with id %lu", id);
	
	stmt = db_get_statement ("itemsetRemoveStmt");
	sqlite3_bind_int (stmt, 1, id);
	res = sqlite3_step (stmt);

	if (SQLITE_DONE != res)
		g_warning ("item remove failed (error code=%d, %s)", res, sqlite3_errmsg (db));
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
		gchar *id = g_strdup( sqlite3_column_text (stmt, 0));
		duplicates = g_slist_append (duplicates, id);
	}

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
	res = sqlite3_step (stmt);

	if (SQLITE_DONE != res)
		g_warning ("removing all items failed (error code=%d, %s)", res, sqlite3_errmsg (db));
}

void
db_item_mark_read (itemPtr item) 
{
	sqlite3_stmt	*stmt;
	gint		res;
	
	item->readStatus = TRUE;
	
	if (!item->validGuid)
	{
		debug1 (DEBUG_DB, "marking item with id=%lu read", item->id);
			
		stmt = db_get_statement ("itemMarkReadStmt");
		sqlite3_bind_int (stmt, 1, item->id);
		res = sqlite3_step (stmt);

		if (SQLITE_DONE != res)
			g_warning ("marking item read failed (error code=%d, %s)", res, sqlite3_errmsg (db));
	}
	else
	{
		debug1 (DEBUG_DB, "marking all duplicates with source id=%s read", item->sourceId);
		
		stmt = db_get_statement ("duplicatesMarkReadStmt");
		sqlite3_bind_text (stmt, 1, item->sourceId, -1, SQLITE_TRANSIENT);
		res = sqlite3_step (stmt);

		if (SQLITE_DONE != res)
			g_warning ("marking duplicates read failed (error code=%d, %s)", res, sqlite3_errmsg (db));
	}
}

void 
db_itemset_mark_all_updated (const gchar *id) 
{
	sqlite3_stmt	*stmt;
	gint		res;
	
	debug1 (DEBUG_DB, "marking all items updared for item set with %s", id);
		
	stmt = db_get_statement ("itemsetMarkAllUpdatedStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);

	if (SQLITE_DONE != res)
		g_warning ("marking all items updated failed (error code=%d, %s)", res, sqlite3_errmsg (db));
}

void 
db_itemset_mark_all_old (const gchar *id) 
{
	sqlite3_stmt	*stmt;
	gint		res;
	
	debug1 (DEBUG_DB, "marking all items old for item set with %s", id);
		
	stmt = db_get_statement ("itemsetMarkAllOldStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step (stmt);

	if (SQLITE_DONE != res)
		g_warning ("marking all items old failed (error code=%d, %s)", res, sqlite3_errmsg(db));
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

	debug_end_measurement (DEBUG_DB, "counting items");
		
	return count;
}

void
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

void
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

void
db_commit_transaction (void)
{
	gchar	*sql, *err;
	gint	res;
	
	sql = sqlite3_mprintf ("COMMIT");
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res) 
		g_warning ("Transaction commit failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);	
}

void
db_rollback_transaction (void) 
{
	gchar	*sql, *err;
	gint	res;
	
	sql = sqlite3_mprintf ("ROLLBACK");
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res) 
		g_warning ("Transaction begin failed (%s) SQL: %s", err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);
}

static gchar *
db_query_to_sql (guint id, const queryPtr query) 
{
	gchar		*sql, *join, *from, *columns, *itemMatch = NULL, *tmp;
	gint		tables, baseTable = 0;
	
	tables = query->tables;
	g_return_val_if_fail (tables != 0, NULL);
	
	/* 1.) determine ROWID column and base table */
	if (tables & QUERY_TABLE_ITEMS) {
		baseTable = QUERY_TABLE_ITEMS;
		tables -= baseTable;
		from = g_strdup ("FROM items ");
	} else if (tables & QUERY_TABLE_METADATA) {
		baseTable = QUERY_TABLE_METADATA;
		tables -= baseTable;
		from = g_strdup ("FROM metadata ");
	} else if (tables & QUERY_TABLE_NODE) {
		baseTable = QUERY_TABLE_NODE;
		tables -= baseTable;
		from = g_strdup ("FROM itemsets INNER JOIN node ON node.node_id = itemsets.node_id ");
	} else {
		g_warning ("Fatal: unknown table constant passed to query construction! (1)");
		return NULL;
	}

	/* 2.) determine select columns */
	
	/* first add id column */
	g_assert (query->columns & QUERY_COLUMN_ITEM_ID);
	if (baseTable == QUERY_TABLE_ITEMS)
		columns = g_strdup ("items.ROWID AS item_id");
	else if (baseTable == QUERY_TABLE_METADATA)
		columns = g_strdup ("metadata.item_id AS item_id");
	else if (baseTable == QUERY_TABLE_NODE)
		columns = g_strdup ("itemsets.item_id AS item_id");
	else {
		g_warning ("Fatal: unknown table constant passed to query construction! (2)");
		return NULL;
	}
	
	if (query->columns & QUERY_COLUMN_ITEM_READ_STATUS) {
		if (query->tables & QUERY_TABLE_ITEMS) {
			tmp = columns;
			columns = g_strdup_printf ("%s,items.read AS item_read", tmp);
			g_free (tmp);
		} else if (query->tables & QUERY_TABLE_NODE) {
			tmp = columns;
			columns = g_strdup_printf ("%s,itemsets.read AS item_read", tmp);
			g_free (tmp);	
		} else {
			//g_warning ("Fatal: neither items nor itemsets included in query tables!");
		}
	}
	
	/* 3.) join remaining tables	 */
	join = g_strdup ("");
	
	/* (tables == QUERY_TABLE_ITEMS) can never happen */
	
	if (tables == QUERY_TABLE_METADATA) {
		tmp = join;
		tables -= QUERY_TABLE_METADATA;
		if (baseTable == QUERY_TABLE_ITEMS) {
			join = g_strdup_printf ("%sINNER JOIN metadata ON items.ROWID = metadata.ROWID ", join);
		} else {
			g_warning ("Fatal: unsupported merge combination: metadata + %d!", baseTable);
			return NULL;
		}
		g_free (tmp);
	}
	if (tables == QUERY_TABLE_NODE) {
		tmp = join;
		tables -= QUERY_TABLE_NODE;
		if (baseTable == QUERY_TABLE_ITEMS) {
			join = g_strdup_printf ("%sINNER JOIN itemsets ON items.ROWID = itemsets.item_id INNER JOIN node ON node.node_id = itemsets.node_id ", join);
		} else if (baseTable == QUERY_TABLE_METADATA) {
			join = g_strdup_printf ("%sINNER JOIN itemsets ON itemsets.item_id = metadata.item_id INNER JOIN node ON node.node_id = itemsets.node_id ", join);
		} else {
			g_warning ("Fatal: unsupported merge combination: node + %d!", baseTable);
			return NULL;
		}
		g_free (tmp);
	}
	g_assert (0 == tables);
	
	/* 4.) create SQL query */
	if (0 != id) {
		if (baseTable == QUERY_TABLE_METADATA)
			itemMatch = g_strdup_printf ("metadata.item_id=%d", id);
		else if (baseTable == QUERY_TABLE_ITEMS)
			itemMatch = g_strdup_printf ("items.ROWID=%d", id);
		else if (baseTable == QUERY_TABLE_NODE)
			itemMatch = g_strdup_printf ("itemsets.item_id=%d", id);
		else {
			g_warning ("Fatal: unknown table constant passed to query construction! (3)");
			return NULL;
		}
	}
			
	if (itemMatch)
		sql = sqlite3_mprintf ("SELECT %s %s %s WHERE (%s AND %s)",
		                       columns, from, join, itemMatch, query->conditions);
	else
		sql = sqlite3_mprintf ("SELECT %s %s %s WHERE (%s)",
		                       columns, from, join, query->conditions);
				       
	g_free (columns);
	g_free (from);
	g_free (join);
	g_free (itemMatch);
	
	return sql;
}

gboolean
db_item_check (guint id, const queryPtr query)
{
	gchar		*sql;
	gint		res;
	sqlite3_stmt	*itemCheckStmt;	

	sql = db_query_to_sql (id, query);
	g_return_val_if_fail (sql != NULL, FALSE);
					       
	db_prepare_stmt (&itemCheckStmt, sql);
	sqlite3_reset (itemCheckStmt);

	res = sqlite3_step (itemCheckStmt);

	sqlite3_free (sql);
	sqlite3_finalize (itemCheckStmt);
	
	return (SQLITE_ROW == res);
}

static void
db_view_create_triggers (const gchar *id)
{
	gchar	*sql, *err;
	gint	res;

	/* we use REPLACE so we need to have before and after INSERT triggers... */
	err = NULL;
	sql = sqlite3_mprintf ("CREATE TRIGGER view_%s_insert_before BEFORE INSERT ON items "
	                       "BEGIN"
			       "   UPDATE view_state SET count = ("
			       "      (SELECT count FROM view_state WHERE node_id = '%s') -"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = new.ROWID)"
			       "   ) WHERE node_id = '%s';"
			       "   UPDATE view_state SET unread = ("
			       "      (SELECT unread FROM view_state WHERE node_id = '%s') -"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = new.ROWID AND item_read = 0)"
			       "   ) WHERE node_id = '%s';"
	                       "END;", id, id, id, id, id, id, id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_warning ("Trigger setup \"view_%s_insert_before\" failed (error code %d: %s) SQL: %s!", id, res, err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);	
	
	err = NULL;
	sql = sqlite3_mprintf ("CREATE TRIGGER view_%s_insert_after AFTER INSERT ON items "
	                       "BEGIN"
			       "   UPDATE view_state SET count = ("
			       "      (SELECT count FROM view_state WHERE node_id = '%s') +"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = new.ROWID)"
			       "   ) WHERE node_id = '%s';"
			       "   UPDATE view_state SET unread = ("
			       "      (SELECT unread FROM view_state WHERE node_id = '%s') +"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = new.ROWID AND item_read = 0)"
			       "   ) WHERE node_id = '%s';"
	                       "END;", id, id, id, id, id, id, id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_warning ("Trigger setup \"view_%s_insert_after\" failed (error code %d: %s) SQL: %s!", id, res, err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);	

	err = NULL;
	sql = sqlite3_mprintf ("CREATE TRIGGER view_%s_delete BEFORE DELETE ON items "
	                       "BEGIN"
			       "   UPDATE view_state SET count = ("
			       "      (SELECT count FROM view_state WHERE node_id = '%s') -"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = old.ROWID)"
			       "   ) WHERE node_id = '%s';"
			       "   UPDATE view_state SET unread = ("
			       "      (SELECT unread FROM view_state WHERE node_id = '%s') -"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = old.ROWID AND item_read = 0)"
			       "   ) WHERE node_id = '%s';"
	                       "END;", id, id, id, id, id, id, id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_warning ("Trigger setup \"view_%s_delete\" failed (error code %d: %s) SQL: %s!", id, res, err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);

	err = NULL;
	sql = sqlite3_mprintf ("CREATE TRIGGER view_%s_update_before BEFORE UPDATE ON items "
	                       "BEGIN"
			       "   UPDATE view_state SET count = ("
			       "      (SELECT count FROM view_state WHERE node_id = '%s') -"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = new.ROWID)"
			       "   ) WHERE node_id = '%s';"
			       "   UPDATE view_state SET unread = ("
			       "      (SELECT unread FROM view_state WHERE node_id = '%s') -"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = new.ROWID AND item_read = 0)"
			       "   ) WHERE node_id = '%s';"
	                       "END;", id, id, id, id, id, id, id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_warning ("Trigger setup \"view_%s_update_before\" failed (error code %d: %s) SQL: %s!", id, res, err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);	

	err = NULL;
	sql = sqlite3_mprintf ("CREATE TRIGGER view_%s_update_after AFTER UPDATE ON items "
	                       "BEGIN"
			       "   UPDATE view_state SET count = ("
			       "      (SELECT count FROM view_state WHERE node_id = '%s') +"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = new.ROWID)"
			       "   ) WHERE node_id = '%s';"
			       "   UPDATE view_state SET unread = ("
			       "      (SELECT unread FROM view_state WHERE node_id = '%s') +"
			       "      (SELECT count(*) FROM view_%s WHERE item_id = new.ROWID AND item_read = 0)"
			       "   ) WHERE node_id = '%s';"
	                       "END;", id, id, id, id, id, id, id);
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_warning ("Trigger setup \"view_%s_update_after\" failed (error code %d: %s) SQL: %s!", id, res, err, sql);
	sqlite3_free (sql);
	sqlite3_free (err);
}

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

void
db_view_create (const gchar *id, queryPtr query)
{
	gchar		*select, *sql, *checkSql, *err = NULL;
	sqlite3_stmt	*viewCheckStmt;
	gint		res;
	gboolean	exists = FALSE;

	/* Prepare SQL for view creation */
	select = db_query_to_sql (0, query);
	if (!select) {
		g_warning ("View query creation failed!");
		return;
	}

	if (query->tables & QUERY_TABLE_NODE)
		sql = sqlite3_mprintf ("CREATE VIEW view_%s AS %s AND itemsets.comment != 1", id, select);
	else if (query->tables & QUERY_TABLE_ITEMS)
		sql = sqlite3_mprintf ("CREATE VIEW view_%s AS %s AND items.comment != 1", id, select);
	else
		sql = sqlite3_mprintf ("CREATE VIEW view_%s AS %s", id, select);
	sqlite3_free (select);
	
	debug2 (DEBUG_DB, "Checking for view %s (SQL=%s)", id, sql);
	
	/* Check if view already exists with exactly the same SQL */
	checkSql = sqlite3_mprintf ("SELECT sql FROM sqlite_master WHERE name = 'view_%s';", id);
	db_prepare_stmt (&viewCheckStmt, checkSql);
	sqlite3_reset (viewCheckStmt);
	res = sqlite3_step (viewCheckStmt);
	if (SQLITE_ROW == res) {
		const gchar *currentSql = sqlite3_column_text (viewCheckStmt, 0);
		if (currentSql) {
			/* Note: this check only works if the above CREATE VIEW
			   SQL statements do not end with a semicolon */
			exists = (0 == strcmp (sql, currentSql));
		}
	}
	sqlite3_finalize (viewCheckStmt);
	sqlite3_free (checkSql);

	if (SQLITE_ROW == res && !exists) {
	
		/* This means there is a view with the same name
		   but not with the expected SQL query. Whatever the
		   reason for this is we do not need it... */
		debug1 (DEBUG_DB, "Found old view with id %s but with wrong SQL, dropping it...", id);
		db_view_remove (id);
	}

	if (!exists) {
		/* Create the view with the prepared SQL */
		res = sqlite3_exec (db, sql, NULL, NULL, &err);
		if (SQLITE_OK != res) 
			debug2 (DEBUG_DB, "Create view failed (%s) SQL: %s", err, sql);
		sqlite3_free (sql);
		sqlite3_free (err);
	} else {
		debug1 (DEBUG_DB, "No need to create view %s as it already exists.", id);
	}
	
	/* Unconditionally recreate the view specific triggers */
	db_view_remove_triggers (id);
	db_view_create_triggers (id);
	
	/* Initialize view counters */
	sql = sqlite3_mprintf ("REPLACE INTO view_state (node_id, unread, count) VALUES ('%s', "
	                       "   (SELECT count(*) FROM view_%s WHERE item_read = 0),"
			       "   (SELECT count(*) FROM view_%s)"
	                       ");", id, id, id); 
	db_exec (sql);
	sqlite3_free (sql);
}

void
db_view_remove (const gchar *id)
{
	gchar	*sql, *err;
	gint	res;
	
	debug1 (DEBUG_DB, "Dropping view \"%s\"", id);
	sql = sqlite3_mprintf ("DROP VIEW view_%s;", id);	
	res = sqlite3_exec (db, sql, NULL, NULL, &err);
	if (SQLITE_OK != res) 
		g_warning ("Dropping view failed (%s) SQL: %s", err, sql);
	
	sqlite3_free (sql);
	sqlite3_free (err);
}

itemSetPtr
db_view_load (const gchar *id) 
{
	gchar		*sql;
	gint		res;
	sqlite3_stmt	*viewLoadStmt;	
	itemSetPtr 	itemSet;

	debug2 (DEBUG_DB, "loading view for node \"%s\" (thread=%p)", id, g_thread_self ());
	
	itemSet = g_new0 (struct itemSet, 1);
	itemSet->nodeId = (gchar *)id;

	sql = sqlite3_mprintf ("SELECT item_id FROM view_%s;", id);
	res = sqlite3_prepare_v2 (db, sql, -1, &viewLoadStmt, NULL);
	sqlite3_free (sql);
	if (SQLITE_OK != res) {
		debug2 (DEBUG_DB, "could not load view %s (error=%d)", id, res);
		return itemSet;
	}

	sqlite3_reset (viewLoadStmt);

	while (sqlite3_step (viewLoadStmt) == SQLITE_ROW) {
		itemSet->ids = g_list_append (itemSet->ids, GUINT_TO_POINTER (sqlite3_column_int (viewLoadStmt, 0)));
	}

	sqlite3_finalize (viewLoadStmt);
	
	debug0 (DEBUG_DB, "loading of view finished");
	
	return itemSet;
}

gboolean
db_view_contains_item (const gchar *id, gulong itemId)
{
	gchar		*sql;
	sqlite3_stmt	*viewCountStmt;	
	gint		res;
	guint		count = 0;

	debug_start_measurement (DEBUG_DB);

	sql = sqlite3_mprintf ("SELECT COUNT(*) FROM view_%s WHERE item_id = %lu;", id, itemId);
	res = sqlite3_prepare_v2 (db, sql, -1, &viewCountStmt, NULL);
	sqlite3_free (sql);
	if (SQLITE_OK != res) {
		debug2 (DEBUG_DB, "couldn't determine view %s item count (error=%d)", id, res);
		return FALSE;
	}
	
	sqlite3_reset (viewCountStmt);
	res = sqlite3_step (viewCountStmt);
	
	if (SQLITE_ROW == res)
		count = sqlite3_column_int (viewCountStmt, 0);
	else
		g_warning ("view item counting failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (viewCountStmt);
	
	debug_end_measurement (DEBUG_DB, "view item counting");
	
	return (count > 0);
}

guint
db_view_get_item_count (const gchar *id)
{
	gchar		*sql;
	sqlite3_stmt	*viewCountStmt;	
	gint		res;
	guint		count = 0;

	debug_start_measurement (DEBUG_DB);

	sql = sqlite3_mprintf ("SELECT count FROM view_state WHERE node_id = '%s';", id);
	res = sqlite3_prepare_v2 (db, sql, -1, &viewCountStmt, NULL);
	sqlite3_free (sql);
	if (SQLITE_OK != res) {
		debug3 (DEBUG_DB, "couldn't determine view %s item count (error=%d, %s)", id, res, sqlite3_errmsg (db));
		return 0;
	}
	
	sqlite3_reset (viewCountStmt);
	res = sqlite3_step (viewCountStmt);
	
	if (SQLITE_ROW == res)
		count = sqlite3_column_int (viewCountStmt, 0);
	else
		g_warning ("view item counting failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (viewCountStmt);
	
	debug_end_measurement (DEBUG_DB, "view item counting");
	
	return count;
}

guint
db_view_get_unread_count (const gchar *id)
{
	gchar		*sql;
	sqlite3_stmt	*viewCountStmt;	
	gint		res;
	guint		count = 0;

	debug_start_measurement (DEBUG_DB);

	sql = sqlite3_mprintf ("SELECT unread FROM view_state WHERE node_id = '%s';", id);
	res = sqlite3_prepare_v2 (db, sql, -1, &viewCountStmt, NULL);
	sqlite3_free (sql);
	if (SQLITE_OK != res) {
		debug2 (DEBUG_DB, "couldn't determine view %s unread count (error=%d)", id, res);
		return 0;
	}
	
	sqlite3_reset (viewCountStmt);
	res = sqlite3_step (viewCountStmt);
	
	if (SQLITE_ROW == res)
		count = sqlite3_column_int (viewCountStmt, 0);
	else
		g_warning ("view unread counting failed (error code=%d, %s)", res, sqlite3_errmsg (db));

	sqlite3_finalize (viewCountStmt);
	
	debug_end_measurement (DEBUG_DB, "view unread counting");
	
	return count;
}

gboolean
db_update_state_load (const gchar *id,
                      updateStatePtr updateState)
{
	sqlite3_stmt	*stmt;
	gint		res;
	
	g_assert (NULL == updateState->lastModified);
	g_assert (NULL == updateState->etag);

	debug2 (DEBUG_DB, "loading subscription %s update state (thread=%p)", id, g_thread_self ());
	debug_start_measurement (DEBUG_DB);	

	stmt = db_get_statement ("updateStateLoadStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);

	res = sqlite3_step (stmt);
	if (SQLITE_ROW == res) {
		updateState->lastModified		= g_strdup (sqlite3_column_text (stmt, 0));
		updateState->etag			= g_strdup (sqlite3_column_text (stmt, 1));
		updateState->lastPoll.tv_sec		= sqlite3_column_int (stmt, 2);
		updateState->lastFaviconPoll.tv_sec	= sqlite3_column_int (stmt, 3);
	} else {
		debug2 (DEBUG_DB, "Could not load update state for subscription %s (error code %d)!", id, res);
	}

	debug_end_measurement (DEBUG_DB, "update state load");
	
	return (SQLITE_ROW == res);
}

void
db_update_state_save (const gchar *id,
                      updateStatePtr updateState)
{
	sqlite3_stmt	*stmt;
	gint		res;

	debug2 (DEBUG_DB, "saving subscription %s update state (thread=%p)", id, g_thread_self ());
	debug_start_measurement (DEBUG_DB);
	
	stmt = db_get_statement ("updateStateSaveStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);

	sqlite3_bind_text (stmt, 2, updateState->lastModified, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 3, updateState->etag, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 4, updateState->lastPoll.tv_sec);
	sqlite3_bind_int  (stmt, 5, updateState->lastFaviconPoll.tv_sec);

	res = sqlite3_step (stmt);
	if (SQLITE_DONE != res)
		g_warning ("Could not save update state for subscription %s (error code %d)!", id, res);

	debug_end_measurement (DEBUG_DB, "update state save");
}

static GSList *
db_subscription_metadata_load(const gchar *id) 
{
	GSList		*metadata = NULL;
	sqlite3_stmt	*stmt;
	gint		res;

	stmt = db_get_statement ("subscriptionMetadataLoadStmt");
	res = sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);
	if (SQLITE_OK != res)
		g_error ("db_load_metadata: sqlite bind failed (error code %d)!", res);

	while (sqlite3_step (stmt) == SQLITE_ROW) {
		metadata = metadata_list_append (metadata, sqlite3_column_text(stmt, 0), 
		                                           sqlite3_column_text(stmt, 1));
	}

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
		g_warning ("Update in \"metadata\" table failed (error code=%d, %s)", res, sqlite3_errmsg (db));
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
	
	debug2 (DEBUG_DB, "updating subscription info %s (thread %p)", subscription->node->id, g_thread_self());
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
		g_warning ("Could not update subscription info %s in DB (error code %d)!", subscription->node->id, res);
		
	db_subscription_metadata_update (subscription);
		
	debug_end_measurement (DEBUG_DB, "subscription_update");
}

void
db_subscription_remove (const gchar *id)
{
	sqlite3_stmt	*stmt;
	gint		res;

	debug2 (DEBUG_DB, "removing subscription %s (thread=%p)", id, g_thread_self ());
	debug_start_measurement (DEBUG_DB);
	
	stmt = db_get_statement ("subscriptionRemoveStmt");
	sqlite3_bind_text (stmt, 1, id, -1, SQLITE_TRANSIENT);

	res = sqlite3_step (stmt);
	if (SQLITE_DONE != res)
		g_warning ("Could not remove subscription %s from DB (error code %d)!", id, res);

	debug_end_measurement (DEBUG_DB, "subscription remove");
}

GSList *
db_subscription_list_load (void)
{
	sqlite3_stmt	*stmt;
	GSList		*list = NULL;

	stmt = db_get_statement ("subscriptionListLoadStmt");
	while (sqlite3_step (stmt) == SQLITE_ROW) {
		list = g_slist_append(list, g_strdup (sqlite3_column_text (stmt, 0)));
	}

	return list;
}

void
db_node_update (nodePtr node)
{
	sqlite3_stmt	*stmt;
	gint		res;
	
	debug2 (DEBUG_DB, "updating node info %s (thread %p)", node->id, g_thread_self());
	debug_start_measurement (DEBUG_DB);
	
	stmt = db_get_statement ("nodeUpdateStmt");
	sqlite3_bind_text (stmt, 1, node->id, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 2, node->parent->id, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text (stmt, 3, node->title, -1, SQLITE_TRANSIENT);	
	sqlite3_bind_text (stmt, 4, node_type_to_str (node), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int  (stmt, 5, node->expanded?1:0);
	sqlite3_bind_int  (stmt, 6, node->viewMode);
	sqlite3_bind_int  (stmt, 7, node->sortColumn);
	sqlite3_bind_int  (stmt, 8, node->sortReversed?1:0);
	
	res = sqlite3_step (stmt);
	if (SQLITE_DONE != res)
		g_warning ("Could not update subscription info %s in DB (error code %d)!", node->id, res);
		
	debug_end_measurement (DEBUG_DB, "subscription_update");
	
}
