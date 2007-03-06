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
static sqlite3_stmt *itemsetMarkAllReadStmt = NULL;
static sqlite3_stmt *itemsetMarkAllUpdatedStmt = NULL;
static sqlite3_stmt *itemsetMarkAllOldStmt = NULL;
static sqlite3_stmt *itemsetMarkAllPopupStmt = NULL;

static sqlite3_stmt *itemLoadStmt = NULL;
static sqlite3_stmt *itemInsertStmt = NULL;
static sqlite3_stmt *itemUpdateStmt = NULL;
static sqlite3_stmt *itemRemoveStmt = NULL;

static sqlite3_stmt *metadataLoadStmt = NULL;
static sqlite3_stmt *metadataInsertStmt = NULL;
static sqlite3_stmt *metadataRemoveStmt = NULL;

static const gchar *schema_items = "\
CREATE TABLE items ( \
	title			TEXT, \
	read			INTEGER, \
	new			INTEGER, \
	updated			INTEGER, \
	popup			INTEGER, \
	marked			INTEGER, \
	source			TEXT, \
	source_id		TEXT, \
	valid_guid		INTEGER, \
	real_source_url		TEXT, \
	real_source_title	TEXT,	\
	description		TEXT, \
	date			INTEGER, \
	comment_feed_id		INTEGER \
);";

static const gchar *schema_itemsets = "\
CREATE TABLE itemsets ( \
	item_id		INTEGER, \
	node_id		TEXT \
); \
CREATE INDEX itemset_idx ON itemsets (node_id);";

static const gchar *schema_metadata = "\
CREATE TABLE metadata ( \
	item_id		INTEGER, \
	nr              INTEGER, \
	key             TEXT, \
	value           TEXT, \
        PRIMARY KEY (item_id, nr) \
); \
CREATE INDEX metadata_idx ON metadata (item_id);";

static void db_prepare_stmt(sqlite3_stmt **stmt, gchar *sql) {
	gint		res;	
	const char	*left;
		
	res = sqlite3_prepare(db, sql, -1, stmt, &left);
	if(SQLITE_OK != res)
		g_error("Failure while preparing statement, (error=%d, %s) SQL: \"%s\"", res, sqlite3_errmsg(db), sql);
}

/* opening or creation of database */
void db_init(void) {
	gchar		*filename;
		
	debug_enter("db_init");

	filename = common_create_cache_filename(NULL, "liferea", "db");
	if(!sqlite3_open(filename, &db)) {
		debug1(DEBUG_CACHE, "Data base file %s was not found... Creating new one.\n", filename);
	}
	g_free(filename);
	
	/* create tables */
	
	sqlite3_exec(db, schema_items,		NULL, NULL, NULL);
	sqlite3_exec(db, schema_itemsets,	NULL, NULL, NULL);
	sqlite3_exec(db, schema_metadata,	NULL, NULL, NULL);

	/* prepare statements */
	
	db_prepare_stmt(&itemsetLoadStmt,
	               "SELECT item_id FROM itemsets WHERE node_id = ?");		      
	
	db_prepare_stmt(&itemsetInsertStmt,
	                "INSERT INTO itemsets (item_id,node_id) VALUES (?,?)");
	
	db_prepare_stmt(&itemsetReadCountStmt,
	               "SELECT COUNT(*) FROM items INNER JOIN itemsets "
	               "ON items.ROWID = itemsets.item_id "
		       "WHERE items.read = 0 AND node_id = ? "
		       "ORDER BY items.date, items.ROWID DESC");
		       
	db_prepare_stmt(&itemsetItemCountStmt,
	               "SELECT COUNT(*) FROM items INNER JOIN itemsets "
	               "ON items.ROWID = itemsets.item_id "
		       "WHERE node_id = ?");
		       
	db_prepare_stmt(&itemsetRemoveStmt,
	                "DELETE FROM itemsets WHERE item_id = ?");
			
	db_prepare_stmt(&itemsetRemoveAllStmt,
	                "DELETE FROM itemsets WHERE node_id = ?");
			
	db_prepare_stmt(&itemsetMarkAllReadStmt,
	                "UPDATE items SET read = 1 WHERE ROWID IN "
			"(SELECT item_id FROM itemsets WHERE node_id = ?)");

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
		       
	db_prepare_stmt(&itemRemoveStmt,
	                "DELETE FROM items WHERE ROWID = ?");
			
	db_prepare_stmt(&metadataLoadStmt,
	                "SELECT key,value,nr FROM metadata WHERE item_id = ? ORDER BY nr");
			
	db_prepare_stmt(&metadataInsertStmt,
	                "REPLACE INTO metadata (item_id,nr,key,value) VALUES (?,?,?,?)");
			
	db_prepare_stmt(&metadataRemoveStmt,
	                "DELETE FROM metadata WHERE item_id = ?");
	
	debug_exit("db_init");
}

void db_deinit(void) {

	debug_enter("db_deinit");
	
	sqlite3_finalize(itemsetLoadStmt);
	sqlite3_finalize(itemsetInsertStmt);
	sqlite3_finalize(itemsetReadCountStmt);
	sqlite3_finalize(itemsetItemCountStmt);
	sqlite3_finalize(itemsetRemoveStmt);
	sqlite3_finalize(itemsetRemoveAllStmt);
	sqlite3_finalize(itemsetMarkAllReadStmt);
	sqlite3_finalize(itemsetMarkAllOldStmt);
	sqlite3_finalize(itemsetMarkAllUpdatedStmt);
	sqlite3_finalize(itemsetMarkAllPopupStmt);
	
	sqlite3_finalize(itemLoadStmt);
	sqlite3_finalize(itemInsertStmt);
	sqlite3_finalize(itemUpdateStmt);
	sqlite3_finalize(itemRemoveStmt);
	
	sqlite3_finalize(metadataLoadStmt);
	sqlite3_finalize(metadataInsertStmt);
	sqlite3_finalize(metadataRemoveStmt);
		
	if(SQLITE_OK != sqlite3_close(db))
		g_warning("DB close failed: %s", sqlite3_errmsg(db));
	
	debug_exit("db_deinit");
}

static GSList * db_metadata_load(gulong id) {
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

static void db_metadata_update_cb(const gchar *key, const gchar *value, guint index, gpointer user_data) {
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

static void db_metadata_update(itemPtr item) {

	metadata_list_foreach(item->metadata, db_metadata_update_cb, item);
}

static void db_metadata_remove(gulong id) {
	gint	res;
	
	sqlite3_reset(metadataRemoveStmt);
	sqlite3_bind_int(metadataRemoveStmt, 1, id);
	res = sqlite3_step(metadataRemoveStmt);
	if(SQLITE_DONE != res) 
		g_warning("Delete in \"metadata\" table failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}
/* Item structure loading methods */

static itemPtr db_load_item_from_columns(sqlite3_stmt *stmt) {
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

itemSetPtr db_itemset_load(const gchar *id) {
	itemSetPtr 	itemSet;
	gint		res;

	debug2(DEBUG_DB, "load of itemset for node \"%s\" (thread=%p)", id, g_thread_self());
	itemSet = g_new0(struct itemSet, 1);
	itemSet->nodeId = (gchar *)id;

	sqlite3_reset(itemsetLoadStmt);
	res = sqlite3_bind_text(itemsetLoadStmt, 1, id, -1, SQLITE_TRANSIENT);
	if(SQLITE_OK != res)
		g_error("db_load_itemset_with_node_id: sqlite bind failed (error code %d)!", res);

	while(sqlite3_step(itemsetLoadStmt) == SQLITE_ROW) {
		itemSet->ids = g_list_append(itemSet->ids, GUINT_TO_POINTER(sqlite3_column_int(itemsetLoadStmt, 0)));
	}

	return itemSet;
}

itemPtr db_item_load(gulong id) {
	itemPtr 	item = NULL;
	gint		res;
	
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

	return item;
}

/* Item modification methods */

static int db_item_set_id_cb(void *user_data, int count, char **values, char **columns) {
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

static void db_item_set_id(itemPtr item) {
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

void db_item_update(itemPtr item) {
	gint	res;
	
	debug3(DEBUG_DB, "update of item \"%s\" (id=%lu, thread=%p)", item->title, item->id, g_thread_self());

	if(!item->id) {
		db_item_set_id(item);

		debug1(DEBUG_DB, "insert into table \"itemsets\": \"%s\"", item->title);	
		
		/* insert item <-> node relation */
		sqlite3_reset(itemsetInsertStmt);
		sqlite3_bind_int(itemsetInsertStmt, 1, item->id);
		sqlite3_bind_text(itemsetInsertStmt, 2, item->nodeId, -1, SQLITE_TRANSIENT);
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
	sqlite3_bind_text(itemUpdateStmt, 10,  item->real_source_url, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(itemUpdateStmt, 11, item->real_source_title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(itemUpdateStmt, 12, item->description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int (itemUpdateStmt, 13, item->time);
	sqlite3_bind_text(itemUpdateStmt, 14, item->commentFeedId, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int (itemUpdateStmt, 15, item->id);
	res = sqlite3_step(itemUpdateStmt);

	if(SQLITE_DONE != res) 
		g_warning("item update failed (error code=%d, %s)", res, sqlite3_errmsg(db));
	
	db_metadata_update(item);
}

void db_item_remove(gulong id) {
	gint	res;
	
	debug1(DEBUG_DB, "removing item with id %lu", id);
	
	db_metadata_remove(id);
	
	sqlite3_reset(itemsetRemoveStmt);
	sqlite3_bind_int(itemsetRemoveStmt, 1, id);
	res = sqlite3_step(itemsetRemoveStmt);

	if(SQLITE_DONE != res)
		g_warning("item remove failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

void db_itemset_remove_all(const gchar *id) {
	gint	res;
	
	debug1(DEBUG_DB, "removing all itesm for item set with %s", id);
		
	sqlite3_reset(itemsetRemoveAllStmt);
	sqlite3_bind_text(itemsetRemoveAllStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetRemoveAllStmt);

	if(SQLITE_DONE != res)
		g_warning("removing all items failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

void db_itemset_mark_all_read(const gchar *id) {
	gint	res;
	
	debug1(DEBUG_DB, "marking all items read for item set with %s", id);
		
	sqlite3_reset(itemsetMarkAllReadStmt);
	sqlite3_bind_text(itemsetMarkAllReadStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetMarkAllReadStmt);

	if(SQLITE_DONE != res)
		g_warning("marking all items read failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

void db_itemset_mark_all_updated(const gchar *id) {
	gint	res;
	
	debug1(DEBUG_DB, "marking all items updared for item set with %s", id);
		
	sqlite3_reset(itemsetMarkAllUpdatedStmt);
	sqlite3_bind_text(itemsetMarkAllUpdatedStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetMarkAllUpdatedStmt);

	if(SQLITE_DONE != res)
		g_warning("marking all items updated failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

void db_itemset_mark_all_old(const gchar *id) {
	gint	res;
	
	debug1(DEBUG_DB, "marking all items old for item set with %s", id);
		
	sqlite3_reset(itemsetMarkAllOldStmt);
	sqlite3_bind_text(itemsetMarkAllOldStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetMarkAllOldStmt);

	if(SQLITE_DONE != res)
		g_warning("marking all items old failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

void db_itemset_mark_all_popup(const gchar *id) {
	gint	res;
	
	debug1(DEBUG_DB, "marking all items popup for item set with %s", id);
		
	sqlite3_reset(itemsetMarkAllPopupStmt);
	sqlite3_bind_text(itemsetMarkAllPopupStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetMarkAllPopupStmt);

	if(SQLITE_DONE != res)
		g_warning("marking all items popup failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

/* Statistics interface */

guint db_itemset_get_unread_count(const gchar *id) {
	gint	res;
	guint	count = 0;
	
	sqlite3_reset(itemsetReadCountStmt);
	sqlite3_bind_text(itemsetReadCountStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetReadCountStmt);
	
	if(SQLITE_ROW == res)
		count = sqlite3_column_int(itemsetReadCountStmt, 0);
	else
		g_warning("item read counting failed (error code=%d, %s)", res, sqlite3_errmsg(db));
		
	return count;
}

guint db_itemset_get_item_count(const gchar *id) {
	gint	res;
	guint	count = 0;
	
	sqlite3_reset(itemsetItemCountStmt);
	sqlite3_bind_text(itemsetItemCountStmt, 1, id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetItemCountStmt);
	
	if(SQLITE_ROW == res)
		count = sqlite3_column_int(itemsetItemCountStmt, 0);
	else
		g_warning("item counting failed (error code=%d, %s)", res, sqlite3_errmsg(db));
		
	return count;
}
