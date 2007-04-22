/**
 * @file db.c sqlite backend for item storage
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

static sqlite3 *db = NULL;

static sqlite3_stmt *itemsetLoadStmt = NULL;
static sqlite3_stmt *itemsetInsertStmt = NULL;
static sqlite3_stmt *itemsetReadCountStmt = NULL;
static sqlite3_stmt *itemsetItemCountStmt = NULL;
static sqlite3_stmt *itemsetRemoveStmt = NULL;

static sqlite3_stmt *itemsetRemoveAllStmt = NULL;
static sqlite3_stmt *itemsetMarkAllUpdatedStmt = NULL;
static sqlite3_stmt *itemsetMarkAllOldStmt = NULL;
static sqlite3_stmt *itemsetMarkAllPopupStmt = NULL;

static sqlite3_stmt *itemLoadStmt = NULL;
static sqlite3_stmt *itemInsertStmt = NULL;
static sqlite3_stmt *itemUpdateStmt = NULL;
static sqlite3_stmt *itemMarkReadStmt = NULL;

static sqlite3_stmt *duplicatesFindStmt = NULL;
static sqlite3_stmt *duplicatesMarkReadStmt = NULL;

static sqlite3_stmt *metadataLoadStmt = NULL;
static sqlite3_stmt *metadataInsertStmt = NULL;
static sqlite3_stmt *metadataRemoveStmt = NULL;

static void
db_prepare_stmt (sqlite3_stmt **stmt, gchar *sql) 
{
	gint		res;	
	const char	*left;
		
	res = sqlite3_prepare(db, sql, -1, stmt, &left);
	if(SQLITE_OK != res)
		g_error("Failure while preparing statement, (error=%d, %s) SQL: \"%s\"", res, sqlite3_errmsg(db), sql);
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

#define SCHEMA_TARGET_VERSION 1

/* opening or creation of database */
void
db_init (void) 
{
	gchar		*filename;
	gint		schemaVersion;
		
	debug_enter ("db_init");

open:
	filename = common_create_cache_filename (NULL, "liferea", "db");
	debug1 (DEBUG_DB, "Opening DB file %s...", filename);
	if (!sqlite3_open (filename, &db)) {
		debug1 (DEBUG_CACHE, "Data base file %s was not found... Creating new one.\n", filename);
	}
	g_free (filename);
	
	/* create info table/check versioning info */				   
	schemaVersion = db_get_schema_version ();
	debug1 (DEBUG_DB, "current DB schema version: %d", schemaVersion);
	
	if (-1 == schemaVersion) {
		/* no schema version available -> first installation
		   without tables or migration from 1.3.2... */
		if (db_table_exists ("itemsets")) {
			db_set_schema_version (0);
			schemaVersion = 0;	/* trigger migration path */
		} else {
			db_set_schema_version (SCHEMA_TARGET_VERSION);
			schemaVersion = SCHEMA_TARGET_VERSION;	/* nothing exists yet, tables will be created below */
		}
	}
	
	if (SCHEMA_TARGET_VERSION < schemaVersion)
		g_error ("Fatal: The cache database was created by a newer version of Liferea than this one!");
		       
	if (SCHEMA_TARGET_VERSION > schemaVersion) {
		/* do table migration */	
		if (0 == schemaVersion) {
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
		
		if (SCHEMA_TARGET_VERSION != db_get_schema_version ())
			g_error ("Fatal: DB schema migration failed! Running with --debug-db could give some hints!");
			
		db_deinit ();			
		debug0 (DEBUG_DB, "Reopening DB after migration...");
		goto open;
	}

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
	         "   comment_feed_id	INTEGER"
	         ");");
			
	db_exec ("CREATE INDEX items_idx ON items (source_id);");

	db_exec ("CREATE TABLE itemsets ("
	         "   item_id		INTEGER,"
	         "   node_id		TEXT,"
	         "   read		INTEGER,"
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
	
	/* Set up removal trigger */	
	db_exec ("DROP TRIGGER item_removal;");
	db_exec ("CREATE TRIGGER item_removal DELETE ON itemsets \
	          BEGIN \
	             DELETE FROM items WHERE ROWID = old.item_id; \
	          END;");
			   
	/* Set up read state update triggers */
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
			   
	/* Cleanup of DB */
	
	db_exec ("DELETE FROM items WHERE ROWID NOT IN "
		 "(SELECT item_id FROM itemsets);");
			  
	db_exec ("DELETE FROM itemsets WHERE item_id NOT IN "
		 "(SELECT ROWID FROM items);");
	
	/*res = sqlite3_exec (db, "PRAGMA synchronous=off", NULL, NULL, &err);
	if (SQLITE_OK != res)
		g_error ("Failure when disabling sync mode, (error=%d, %s)", res, err);
	sqlite3_free(err);*/

	/* prepare statements */
	
	db_prepare_stmt(&itemsetLoadStmt,
	               "SELECT item_id FROM itemsets WHERE node_id = ?");
		       
	db_prepare_stmt(&itemsetInsertStmt,
	                "INSERT INTO itemsets (item_id,node_id,read) VALUES (?,?,?)");
	
	db_prepare_stmt(&itemsetReadCountStmt,
	               "SELECT COUNT(*) FROM itemsets "
		       "WHERE node_id = ? AND read = 0");
	       
	db_prepare_stmt(&itemsetItemCountStmt,
	               "SELECT COUNT(*) FROM itemsets "
		       "WHERE node_id = ?");
		       
	db_prepare_stmt(&itemsetRemoveStmt,
	                "DELETE FROM itemsets WHERE item_id = ?");
			
	db_prepare_stmt(&itemsetRemoveAllStmt,
	                "DELETE FROM itemsets WHERE node_id = ?");

	db_prepare_stmt(&itemsetMarkAllUpdatedStmt,
	                "UPDATE items SET updated = 0 WHERE ROWID IN "
			"(SELECT item_id FROM itemsets WHERE node_id = ?)");
			
	db_prepare_stmt(&itemsetMarkAllOldStmt,
	                "UPDATE items SET new = 0 WHERE ROWID IN "
			"(SELECT item_id FROM itemsets WHERE node_id = ?)");

	db_prepare_stmt(&itemsetMarkAllPopupStmt,
	                "UPDATE items SET popup = 0 WHERE ROWID IN "
			"(SELECT item_id FROM itemsets WHERE node_id = ?)");		

	db_prepare_stmt(&itemLoadStmt,	
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
		       "itemsets.item_id,"
		       "itemsets.node_id"
	               " FROM items INNER JOIN itemsets "
	               "ON items.ROWID = itemsets.item_id "
	               "WHERE items.ROWID = ?");      
	
	db_prepare_stmt(&itemUpdateStmt,
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
	               "ROWID"
	               ") values (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
			
	db_prepare_stmt(&itemMarkReadStmt,
	                "UPDATE items SET read = 1 WHERE ROWID = ?");
					
	db_prepare_stmt(&duplicatesFindStmt,
	                "SELECT ROWID FROM items WHERE source_id = ?");
		       
	db_prepare_stmt(&duplicatesMarkReadStmt,
 	                "UPDATE items SET read = 1 WHERE source_id = ?");
						
	db_prepare_stmt(&metadataLoadStmt,
	                "SELECT key,value,nr FROM metadata WHERE item_id = ? ORDER BY nr");
			
	db_prepare_stmt(&metadataInsertStmt,
	                "REPLACE INTO metadata (item_id,nr,key,value) VALUES (?,?,?,?)");
			
	db_prepare_stmt(&metadataRemoveStmt,
	                "DELETE FROM metadata WHERE item_id = ?");
	
	debug_exit("db_init");
}

void
db_deinit (void) 
{

	debug_enter("db_deinit");
	
	sqlite3_finalize(itemsetLoadStmt);
	sqlite3_finalize(itemsetInsertStmt);
	sqlite3_finalize(itemsetReadCountStmt);
	sqlite3_finalize(itemsetItemCountStmt);
	sqlite3_finalize(itemsetRemoveStmt);

	sqlite3_finalize(itemsetRemoveAllStmt);
	sqlite3_finalize(itemsetMarkAllOldStmt);
	sqlite3_finalize(itemsetMarkAllUpdatedStmt);
	sqlite3_finalize(itemsetMarkAllPopupStmt);
	
	sqlite3_finalize(itemLoadStmt);
	sqlite3_finalize(itemInsertStmt);
	sqlite3_finalize(itemUpdateStmt);
	sqlite3_finalize(itemMarkReadStmt);
	
	sqlite3_finalize(duplicatesFindStmt);
	sqlite3_finalize(duplicatesMarkReadStmt);
	
	sqlite3_finalize(metadataLoadStmt);
	sqlite3_finalize(metadataInsertStmt);
	sqlite3_finalize(metadataRemoveStmt);
		
	if(SQLITE_OK != sqlite3_close(db))
		g_warning("DB close failed: %s", sqlite3_errmsg(db));
	
	db = NULL;
	
	debug_exit("db_deinit");
}

static GSList *
db_metadata_load(gulong id) 
{
	GSList	*metadata = NULL;
	gint	res;

	sqlite3_reset(metadataLoadStmt);
	res = sqlite3_bind_int(metadataLoadStmt, 1, id);
	if(SQLITE_OK != res)
		g_error("db_load_metadata: sqlite bind failed (error code %d)!", res);

	while(sqlite3_step(metadataLoadStmt) == SQLITE_ROW) {
		metadata = metadata_list_append(metadata, sqlite3_column_text(metadataLoadStmt, 0), 
		                                          sqlite3_column_text(metadataLoadStmt, 1));
	}

	return metadata;
}

static void
db_metadata_update_cb (const gchar *key,
                       const gchar *value,
                       guint index,
                       gpointer user_data) 
{
	itemPtr	item = (itemPtr)user_data;
	gint	res;

	sqlite3_reset(metadataInsertStmt);
	sqlite3_bind_int (metadataInsertStmt, 1, item->id);
	sqlite3_bind_int (metadataInsertStmt, 2, index);
	sqlite3_bind_text(metadataInsertStmt, 3, key, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(metadataInsertStmt, 4, value, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(metadataInsertStmt);
	if(SQLITE_DONE != res) 
		g_warning("Update in \"metadata\" table failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

static void
db_metadata_update(itemPtr item) 
{
	metadata_list_foreach(item->metadata, db_metadata_update_cb, item);
}

static void
db_metadata_remove(gulong id) 
{
	gint	res;
	
	sqlite3_reset(metadataRemoveStmt);
	sqlite3_bind_int(metadataRemoveStmt, 1, id);
	res = sqlite3_step(metadataRemoveStmt);
	if(SQLITE_DONE != res) 
		g_warning("Delete in \"metadata\" table failed (error code=%d, %s)", res, sqlite3_errmsg(db));
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
	item->id		= sqlite3_column_int(stmt, 14);
	item->nodeId		= g_strdup(sqlite3_column_text(stmt, 15));

	item_set_title			(item, sqlite3_column_text(stmt, 0));
	item_set_source			(item, sqlite3_column_text(stmt, 6));
	item_set_id			(item, sqlite3_column_text(stmt, 7));	
	item_set_real_source_url	(item, sqlite3_column_text(stmt, 9));
	item_set_real_source_title	(item, sqlite3_column_text(stmt, 10));
	item_set_description		(item, sqlite3_column_text(stmt, 11));

	item->metadata = db_metadata_load(item->id);

	return item;
}

itemSetPtr
db_itemset_load (const gchar *id) 
{
	itemSetPtr 	itemSet;
	gint		res;

	debug2(DEBUG_DB, "loading itemset for node \"%s\" (thread=%p)", id, g_thread_self());
	itemSet = g_new0(struct itemSet, 1);
	itemSet->nodeId = (gchar *)id;

	sqlite3_reset(itemsetLoadStmt);
	res = sqlite3_bind_text(itemsetLoadStmt, 1, id, -1, SQLITE_TRANSIENT);
	if(SQLITE_OK != res)
		g_error("db_itemset_load: sqlite bind failed (error code %d)!", res);

	while(sqlite3_step(itemsetLoadStmt) == SQLITE_ROW) {
		itemSet->ids = g_list_append(itemSet->ids, GUINT_TO_POINTER(sqlite3_column_int(itemsetLoadStmt, 0)));
	}

	debug0(DEBUG_DB, "loading of itemset finished");
	
	return itemSet;
}

itemPtr
db_item_load (gulong id) 
{
	itemPtr 	item = NULL;
	gint		res;

	debug2 (DEBUG_DB, "loading item %lu (thread=%p)", id, g_thread_self ());
	debug_start_measurement (DEBUG_DB);
	
	sqlite3_reset(itemLoadStmt);
	res = sqlite3_bind_int(itemLoadStmt, 1, id);
	if(SQLITE_OK != res)
		g_error("db_load_item_with_id: sqlite bind failed (error code %d)!", res);

	if(sqlite3_step(itemLoadStmt) == SQLITE_ROW) {
		item = db_load_item_from_columns(itemLoadStmt);
		res = sqlite3_step(itemLoadStmt);
		/* FIXME: sometimes (after updates) we get an unexpected SQLITE_ROW here! 
		  if(SQLITE_DONE != res)
			g_warning("Unexpected result when retrieving single item id=%lu! (error code=%d, %s)", id, res, sqlite3_errmsg(db));
		 */
	} else {
		debug2(DEBUG_DB, "Could not load item with id #%lu (error code %d)!", id, res);
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
	
	g_assert(0 == item->id);
	
	sql = sqlite3_mprintf("SELECT MAX(ROWID) FROM items");
	res = sqlite3_exec(db, sql, db_item_set_id_cb, item, &err);
	if(SQLITE_OK != res) 
		g_warning("Select failed (%s) SQL: %s", err, sql);
	sqlite3_free(sql);
	sqlite3_free(err);
}

void
db_item_update (itemPtr item) 
{
	gint	res;
	
	debug3 (DEBUG_DB, "update of item \"%s\" (id=%lu, thread=%p)", item->title, item->id, g_thread_self());
	debug_start_measurement (DEBUG_DB);

	if(!item->id) {
		db_item_set_id(item);

		debug1(DEBUG_DB, "insert into table \"itemsets\": \"%s\"", item->title);	
		
		/* insert item <-> node relation */
		sqlite3_reset(itemsetInsertStmt);
		sqlite3_bind_int (itemsetInsertStmt, 1, item->id);
		sqlite3_bind_text(itemsetInsertStmt, 2, item->nodeId, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int (itemsetInsertStmt, 3, item->readStatus);
		res = sqlite3_step(itemsetInsertStmt);
		if(SQLITE_DONE != res) 
			g_warning("Insert in \"itemsets\" table failed (error code=%d, %s)", res, sqlite3_errmsg(db));

	}

	/* Update the item... */
	sqlite3_reset(itemUpdateStmt);
	sqlite3_bind_text(itemUpdateStmt, 1,  item->title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int (itemUpdateStmt, 2,  item->readStatus?1:0);
	sqlite3_bind_int (itemUpdateStmt, 3,  item->newStatus?1:0);
	sqlite3_bind_int (itemUpdateStmt, 4,  item->updateStatus?1:0);
	sqlite3_bind_int (itemUpdateStmt, 5,  item->popupStatus?1:0);
	sqlite3_bind_int (itemUpdateStmt, 6,  item->flagStatus?1:0);
	sqlite3_bind_text(itemUpdateStmt, 7,  item->source, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(itemUpdateStmt, 8,  item->sourceId, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int (itemUpdateStmt, 9,  item->validGuid?1:0);
	sqlite3_bind_text(itemUpdateStmt, 10, item->real_source_url, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(itemUpdateStmt, 11, item->real_source_title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(itemUpdateStmt, 12, item->description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int (itemUpdateStmt, 13, item->time);
	sqlite3_bind_text(itemUpdateStmt, 14, item->commentFeedId, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int (itemUpdateStmt, 15, item->id);
	res = sqlite3_step(itemUpdateStmt);

	if(SQLITE_DONE != res) 
		g_warning("item update failed (error code=%d, %s)", res, sqlite3_errmsg(db));
	
	db_metadata_update(item);
	
	debug_end_measurement (DEBUG_DB, "item update");
}

void
db_item_remove (gulong id) 
{
	gint	res;
	
	debug1(DEBUG_DB, "removing item with id %lu", id);
	
	db_metadata_remove(id);
	
	sqlite3_reset(itemsetRemoveStmt);
	sqlite3_bind_int(itemsetRemoveStmt, 1, id);
	res = sqlite3_step(itemsetRemoveStmt);

	if(SQLITE_DONE != res)
		g_warning("item remove failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

GSList * 
db_item_get_duplicates (const gchar *guid) 
{
	GSList	*duplicates = NULL;
	gint	res;

	debug_start_measurement (DEBUG_DB);

	sqlite3_reset (duplicatesFindStmt);
	res = sqlite3_bind_text (duplicatesFindStmt, 1, guid, -1, SQLITE_TRANSIENT);
	if (SQLITE_OK != res)
		g_error ("db_item_get_duplicates: sqlite bind failed (error code %d)!", res);

	while (sqlite3_step (duplicatesFindStmt) == SQLITE_ROW) 
	{
		gulong id = sqlite3_column_int(duplicatesFindStmt, 0);
		duplicates = g_slist_append (duplicates, GUINT_TO_POINTER (id));
	}

	debug_end_measurement (DEBUG_DB, "searching for duplicates");
	
	return duplicates;
}

void 
db_itemset_remove_all (const gchar *id) 
{
	gint	res;
	
	debug1(DEBUG_DB, "removing all items for item set with %s", id);
		
	sqlite3_reset(itemsetRemoveAllStmt);
	sqlite3_bind_text(itemsetRemoveAllStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetRemoveAllStmt);

	if(SQLITE_DONE != res)
		g_warning("removing all items failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

void
db_item_mark_read (itemPtr item) 
{
	gint	res;
	
	if (!item->validGuid)
	{
		debug1 (DEBUG_DB, "marking item with id=%lu read", item->id);
			
		sqlite3_reset (itemMarkReadStmt);
		sqlite3_bind_int (itemMarkReadStmt, 1, item->id);
		res = sqlite3_step (itemMarkReadStmt);

		if (SQLITE_DONE != res)
			g_warning ("marking item read failed (error code=%d, %s)", res, sqlite3_errmsg (db));
	}
	else
	{
		debug1 (DEBUG_DB, "marking all duplicates with source id=%s read", item->sourceId);
		
		sqlite3_reset (duplicatesMarkReadStmt);
		sqlite3_bind_text (duplicatesMarkReadStmt, 1, item->sourceId, -1, SQLITE_TRANSIENT);
		res = sqlite3_step (duplicatesMarkReadStmt);

		if (SQLITE_DONE != res)
			g_warning ("marking duplicates read failed (error code=%d, %s)", res, sqlite3_errmsg (db));
	}
}

void 
db_itemset_mark_all_updated (const gchar *id) 
{
	gint	res;
	
	debug1(DEBUG_DB, "marking all items updared for item set with %s", id);
		
	sqlite3_reset(itemsetMarkAllUpdatedStmt);
	sqlite3_bind_text(itemsetMarkAllUpdatedStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetMarkAllUpdatedStmt);

	if(SQLITE_DONE != res)
		g_warning("marking all items updated failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

void 
db_itemset_mark_all_old (const gchar *id) 
{
	gint	res;
	
	debug1(DEBUG_DB, "marking all items old for item set with %s", id);
		
	sqlite3_reset(itemsetMarkAllOldStmt);
	sqlite3_bind_text(itemsetMarkAllOldStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetMarkAllOldStmt);

	if(SQLITE_DONE != res)
		g_warning("marking all items old failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

void 
db_itemset_mark_all_popup (const gchar *id) 
{
	gint	res;
	
	debug1(DEBUG_DB, "marking all items popup for item set with %s", id);
		
	sqlite3_reset(itemsetMarkAllPopupStmt);
	sqlite3_bind_text(itemsetMarkAllPopupStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetMarkAllPopupStmt);

	if(SQLITE_DONE != res)
		g_warning("marking all items popup failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

/* Statistics interface */

guint 
db_itemset_get_unread_count (const gchar *id) 
{
	gint	res;
	guint	count = 0;
	
	debug_start_measurement (DEBUG_DB);
	
	sqlite3_reset(itemsetReadCountStmt);
	sqlite3_bind_text(itemsetReadCountStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetReadCountStmt);
	
	if(SQLITE_ROW == res)
		count = sqlite3_column_int(itemsetReadCountStmt, 0);
	else
		g_warning("item read counting failed (error code=%d, %s)", res, sqlite3_errmsg(db));
		
	debug_end_measurement (DEBUG_DB, "counting unread items");

	return count;
}

guint 
db_itemset_get_item_count (const gchar *id) 
{
	gint	res;
	guint	count = 0;

	debug_start_measurement (DEBUG_DB);
	
	sqlite3_reset(itemsetItemCountStmt);
	sqlite3_bind_text(itemsetItemCountStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetItemCountStmt);
	
	if(SQLITE_ROW == res)
		count = sqlite3_column_int(itemsetItemCountStmt, 0);
	else
		g_warning("item counting failed (error code=%d, %s)", res, sqlite3_errmsg(db));

	debug_end_measurement (DEBUG_DB, "counting items");
		
	return count;
}

void
db_begin_transaction (void)
{
	gchar	*sql, *err;
	gint	res;
	
	sql = sqlite3_mprintf("BEGIN");
	res = sqlite3_exec(db, sql, NULL, NULL, &err);
	if(SQLITE_OK != res) 
		g_warning("Transaction begin failed (%s) SQL: %s", err, sql);
	sqlite3_free(sql);
	sqlite3_free(err);
}

void
db_end_transaction (void) 
{
	gchar	*sql, *err;
	gint	res;
	
	sql = sqlite3_mprintf("END");
	res = sqlite3_exec(db, sql, NULL, NULL, &err);
	  if(SQLITE_OK != res) 
		g_warning("Transaction begin failed (%s) SQL: %s", err, sql);
	sqlite3_free(sql);
	sqlite3_free(err);
}

void
db_rollback_transaction (void) 
{
	gchar	*sql, *err;
	gint	res;
	
	sql = sqlite3_mprintf("ROLLBACK");
	res = sqlite3_exec(db, sql, NULL, NULL, &err);
	if(SQLITE_OK != res) 
		g_warning("Transaction begin failed (%s) SQL: %s", err, sql);
	sqlite3_free(sql);
	sqlite3_free(err);
}

gboolean
db_item_check (guint id, const queryPtr query)
{
	gchar		*sql, *tables = NULL;
	gint		res;
	sqlite3_stmt	*itemCheckStmt;	

	g_return_val_if_fail (query->tables != 0, FALSE);
	// g_return_val_if_fail (query->columns == NULL, FALSE); FIXME
		
	if (query->tables == QUERY_TABLE_ITEMS)
		tables = g_strdup ("items.ROWID FROM items");
	if (query->tables == QUERY_TABLE_METADATA)
		tables = g_strdup ("metadata.ROWID FROM metadata");
	if (query->tables == (QUERY_TABLE_METADATA | QUERY_TABLE_ITEMS))
		tables = g_strdup ("items.ROWID FROM items INNER JOIN metadata ON items.ROWID = metadata.item_id");

	sql = sqlite3_mprintf ("SELECT %s WHERE items.ROWID=%d AND %s;",
	                       tables, id, query->conditions);
	db_prepare_stmt (&itemCheckStmt, sql);
	sqlite3_reset (itemCheckStmt);

	res = sqlite3_step (itemCheckStmt);

	g_free (tables);
	sqlite3_free (sql);
	sqlite3_finalize (itemCheckStmt);
	
	return (SQLITE_ROW == res);
}

void
db_view_create (const gchar *id, queryPtr query)
{
	gchar	*sql, *err, *tables = NULL;
	gint	res;

	g_return_if_fail (query->tables != 0);
	// g_return_if_fail (query->columns == NULL); FIXME
		
	if (query->tables & QUERY_TABLE_ITEMS)
		tables = g_strdup ("items");
	if (query->tables & QUERY_TABLE_METADATA)
		tables = g_strdup ("metadata");
	if (query->tables & (QUERY_TABLE_METADATA | QUERY_TABLE_ITEMS))
		tables = g_strdup ("items INNER JOIN metadata ON items.ROWID = metadata.item_id");

	sql = sqlite3_mprintf ("CREATE TEMP VIEW view_%s AS "
	                       "SELECT "
	                       "items.ROWID AS item_id,"
	                       "items.title,"
	                       "items.read,"
	                       "items.updated,"
	                       "items.marked"
			       " FROM %s "
			       "WHERE %s;", 
			       id, tables, query->conditions);

	res = sqlite3_exec(db, sql, NULL, NULL, &err);
	if(SQLITE_OK != res) 
		g_warning("Create view failed (%s) SQL: %s", err, sql);
	
	g_free (tables);
	sqlite3_free (sql);
	sqlite3_free (err);
}

void
db_view_remove (const gchar *id)
{
	gchar	*sql, *err;
	gint	res;
		
	sql = sqlite3_mprintf ("DROP VIEW view_%s;", id);
	
		res = sqlite3_exec(db, sql, NULL, NULL, &err);
	if(SQLITE_OK != res) 
		g_warning("Dropping view failed (%s) SQL: %s", err, sql);
	
	sqlite3_free (sql);
	sqlite3_free (err);
}

itemSetPtr
db_view_load (const gchar *id) 
{
	gchar		*sql;
	sqlite3_stmt	*viewLoadStmt;	
	itemSetPtr 	itemSet;

	debug2 (DEBUG_DB, "loading view for node \"%s\" (thread=%p)", id, g_thread_self());
	itemSet = g_new0 (struct itemSet, 1);
	itemSet->nodeId = (gchar *)id;

	sql = sqlite3_mprintf ("SELECT item_id FROM view_%s;", id);
	db_prepare_stmt (&viewLoadStmt, sql);
	sqlite3_reset (viewLoadStmt);

	while (sqlite3_step (viewLoadStmt) == SQLITE_ROW) {
		itemSet->ids = g_list_append (itemSet->ids, GUINT_TO_POINTER (sqlite3_column_int (viewLoadStmt, 0)));
	}

	sqlite3_free (sql);
	sqlite3_finalize(viewLoadStmt);
	
	debug0 (DEBUG_DB, "loading of view finished");
	
	return itemSet;
}
