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

#include "db.h"
#include "debug.h"
#include "common.h"

static sqlite3 *db = NULL;

static sqlite3_stmt *itemLoadStmt = NULL;
static sqlite3_stmt *itemInsertStmt = NULL;
static sqlite3_stmt *itemUpdateStmt = NULL;
static sqlite3_stmt *itemsetLoadStmt = NULL;
static sqlite3_stmt *itemsetInsertStmt = NULL;

static const gchar *schema_items = "\
CREATE TABLE items ( \
	title			TEXT, \
	read			INTEGER, \
	new			INTEGER, \
	updated			INTEGER, \
	marked			INTEGER, \
	source			TEXT, \
	source_id		TEXT, \
	valid_guid		INTEGER, \
	real_source_url		TEXT, \
	real_source_title	TEXT,	\
	description		TEXT, \
	date			INTEGER \
);";

static const gchar *schema_itemsets = "\
CREATE TABLE itemsets ( \
     item_id	INTEGER, \
     node_id	TEXT \
); \
CREATE INDEX itemset_idx ON itemsets (node_id);";

/* opening or creation of database */
void db_init(void) {
	gchar		*filename, *sql;
	const char	*left;
	gint		res;	
		
	debug_enter("db_init");

	filename = common_create_cache_filename(NULL, "liferea", "db");
	if(!sqlite3_open(filename, &db)) {
		debug1(DEBUG_CACHE, "Data base file %s was not found... Creating new one.\n", filename);
	}
	g_free(filename);
	
	/* create tables */
	
	sqlite3_exec(db, schema_items,		NULL, NULL, NULL);
	sqlite3_exec(db, schema_itemsets,	NULL, NULL, NULL);

	/* prepare statements */
		
	sql = g_strdup("SELECT "
	               "items.title,"
	               "items.read,"
	               "items.new,"
	               "items.updated,"
	               "items.marked,"
	               "items.source,"
	               "items.source_id,"
	               "items.valid_guid,"
	               "items.real_source_url,"
	               "items.real_source_title,"
	               "items.description,"
	               "items.date,"
		       "itemsets.item_id,"
		       "itemsets.node_id"
	               " FROM items INNER JOIN itemsets "
	               "ON items.ROWID = itemsets.item_id "
	               "WHERE itemsets.node_id = ?");		      
	res = sqlite3_prepare(db, sql, -1, &itemsetLoadStmt, &left);
	if(SQLITE_OK != res)
		g_error("Failure while preparing statement, error=%d SQL: \"%s\"", res, sql);
	g_free(sql);
	
	sql = g_strdup("INSERT INTO itemsets (item_id,node_id) VALUES (?,?)");
	res = sqlite3_prepare(db, sql, -1, &itemsetInsertStmt, &left);
	if(SQLITE_OK != res) 
		g_error("Failure while preparing statement, error=%d SQL: \"%s\"", res, sql);
	g_free(sql);
	
	sql = g_strdup("SELECT "
	               "items.title,"
	               "items.read,"
	               "items.new,"
	               "items.updated,"
	               "items.marked,"
	               "items.source,"
	               "items.source_id,"
	               "items.valid_guid,"
	               "items.real_source_url,"
	               "items.real_source_title,"
	               "items.description,"
	               "items.date,"
		       "itemsets.item_id,"
		       "itemsets.node_id"
	               " FROM items INNER JOIN itemsets "
	               "ON items.ROWID = itemsets.item_id "
	               "WHERE items.ROWID = ?");      
	res = sqlite3_prepare(db, sql, -1, &itemLoadStmt, &left);
	if(SQLITE_OK != res)
		g_error("Failure while preparing statement, error=%d SQL: \"%s\"", res, sql);
	g_free(sql);
	
	sql = g_strdup("INSERT INTO items ("
	               "title,"
	               "read,"
	               "new,"
	               "updated,"
	               "marked,"
	               "source,"
	               "source_id,"
	               "valid_guid,"
	               "real_source_url,"
	               "real_source_title,"
	               "description,"
	               "date,"
	               "ROWID"
	               ") values (?,?,?,?,?,?,?,?,?,?,?,?,?)");
	res = sqlite3_prepare(db, sql, -1, &itemInsertStmt, &left);
	if(SQLITE_OK != res) 
		g_error("Failure while preparing statement, error=%d SQL: \"%s\"", res, sql);
	g_free(sql);
	
	sql = g_strdup("UPDATE items SET "
	               "title=?,"
	               "read=?,"
	               "new=?,"
	               "updated=?,"
	               "marked=?,"
	               "source=?,"
	               "source_id=?,"
	               "valid_guid=?,"
	               "real_source_url=?,"
	               "real_source_title=?,"
	               "description=?,"
	               "date=? "
	               "WHERE ROWID=?");
	res = sqlite3_prepare(db, sql, -1, &itemUpdateStmt, &left);
	if(SQLITE_OK != res) 
		g_error("Failure while preparing statement, error=%d SQL: \"%s\"", res, sql);
	g_free(sql);
	
	debug_exit("db_init");
}

void db_deinit(void) {

	debug_enter("db_deinit");

	sqlite3_finalize(itemLoadStmt);
	sqlite3_finalize(itemInsertStmt);
	sqlite3_finalize(itemUpdateStmt);
	sqlite3_finalize(itemsetLoadStmt);
	sqlite3_finalize(itemsetInsertStmt);
		
	sqlite3_close(db);
	
	debug_exit("db_deinit");
}

/* Item structure loading methods */

static itemPtr db_load_item_from_columns(sqlite3_stmt *stmt) {
	itemPtr item = item_new();
	
	item->readStatus	= sqlite3_column_int(stmt, 1)?TRUE:FALSE;
	item->newStatus		= sqlite3_column_int(stmt, 2)?TRUE:FALSE;
	item->updateStatus	= sqlite3_column_int(stmt, 3)?TRUE:FALSE;
	item->flagStatus	= sqlite3_column_int(stmt, 4)?TRUE:FALSE;
	item->validGuid		= sqlite3_column_int(stmt, 7)?TRUE:FALSE;
	item->time		= sqlite3_column_int(stmt, 11);
	item->id		= sqlite3_column_int(stmt, 12);
	item->node		= node_from_id(sqlite3_column_text(stmt, 13));
	item->itemSet		= item->node->itemSet;

	item_set_title			(item, sqlite3_column_text(stmt, 0));
	item_set_source			(item, sqlite3_column_text(stmt, 5));
	item_set_id			(item, sqlite3_column_text(stmt, 6));	
	item_set_real_source_url	(item, sqlite3_column_text(stmt, 8));
	item_set_real_source_title	(item, sqlite3_column_text(stmt, 9));
	item_set_description		(item, sqlite3_column_text(stmt, 10));

	return item;
}

itemSetPtr db_load_itemset_with_node_id(const gchar *id) {
	itemSetPtr 	itemSet;
	gint		res;

	debug2(DEBUG_DB, "load of itemset for node \"%s\" (thread=%p)", id, g_thread_self());
	itemSet = g_new0(struct itemSet, 1);
	itemSet->node = node_from_id(id);

	sqlite3_reset(itemsetLoadStmt);
	res = sqlite3_bind_text(itemsetLoadStmt, 1, id, -1, SQLITE_TRANSIENT);
	if(SQLITE_OK != res)
		g_error("db_load_itemset_with_node_id: sqlite bind failed (error code %d)!", res);

	while(sqlite3_step(itemsetLoadStmt) == SQLITE_ROW) {
		itemset_append_item(itemSet, db_load_item_from_columns(itemsetLoadStmt));
	}

	return itemSet;
}

itemPtr db_load_item_with_id(gulong id) {
	itemPtr 	item = NULL;
	gint		res;
	
	sqlite3_reset(itemLoadStmt);
	res = sqlite3_bind_int(itemLoadStmt, 1, id);
	if(SQLITE_OK != res)
		g_error("db_load_item_with_id: sqlite bind failed (error code %d)!", res);

	if(sqlite3_step(itemLoadStmt) == SQLITE_ROW) {
		item = db_load_item_from_columns(itemLoadStmt);
		g_assert(SQLITE_DONE == sqlite3_step(itemLoadStmt));
	} else {
		debug2(DEBUG_DB, "Could not load item with id #%lu (error code %d)!", id, res);
	}

	return item;
}

/* Item modification methods */

static int db_new_item_id_cb(void *user_data, int count, char **values, char **columns) {
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

void db_set_item_id(itemPtr item) {
	gchar	*sql, *err;
	gint	res;
	
	g_assert(0 == item->id);
	
	sql = sqlite3_mprintf("SELECT MAX(ROWID) FROM items");
	res = sqlite3_exec(db, sql, db_new_item_id_cb, item, &err);
	if(SQLITE_OK != res) 
		g_warning("Select failed (%s) SQL: %s", err, sql);
	sqlite3_free(sql);
	sqlite3_free(err);
}

static void db_insert_item(itemPtr item) {
	gint	res;

	debug1(DEBUG_DB, "insert of item \"%s\"", item->title);	
	g_assert(0 != item->id);
	g_assert(NULL != item->node);
	
	/* insert item <-> node relation */
	sqlite3_reset(itemsetInsertStmt);
	sqlite3_bind_int(itemsetInsertStmt, 1, item->id);
	sqlite3_bind_text(itemsetInsertStmt, 2, item->node->id, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(itemsetInsertStmt);
	if(SQLITE_DONE != res) 
		g_warning("Insert in \"itemsets\" table failed (error code=%d, %s)", res, sqlite3_errmsg(db));
				
	/* insert item data */
	sqlite3_reset(itemInsertStmt);
	sqlite3_bind_text(itemInsertStmt, 1,  item->title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int (itemInsertStmt, 2,  item->readStatus?1:0);
	sqlite3_bind_int (itemInsertStmt, 3,  item->newStatus?1:0);
	sqlite3_bind_int (itemInsertStmt, 4,  item->updateStatus?1:0);
	sqlite3_bind_int (itemInsertStmt, 5,  item->flagStatus?1:0);
	sqlite3_bind_text(itemInsertStmt, 6,  item->source, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(itemInsertStmt, 7,  item->sourceId, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int (itemInsertStmt, 8,  item->validGuid?1:0);
	sqlite3_bind_text(itemInsertStmt, 9,  item->real_source_url, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(itemInsertStmt, 10, item->real_source_title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(itemInsertStmt, 11, item->description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int (itemInsertStmt, 12, item->time);
	sqlite3_bind_int (itemInsertStmt, 13, item->id);
	res = sqlite3_step(itemInsertStmt);
	if(SQLITE_DONE != res) 
		g_warning("Insert in \"items\" table failed (error code=%d, %s)", res, sqlite3_errmsg(db));
}

void db_update_item(itemPtr item) {

	debug3(DEBUG_DB, "update of item \"%s\" (id=%lu, thread=%p)", item->title, item->id, g_thread_self());
	
	if(item->id) {
		gint	res;
		
		/* Try to update the item... */
		sqlite3_reset(itemUpdateStmt);
		sqlite3_bind_text(itemUpdateStmt, 1,  item->title, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int (itemUpdateStmt, 2,  item->readStatus?1:0);
		sqlite3_bind_int (itemUpdateStmt, 3,  item->newStatus?1:0);
		sqlite3_bind_int (itemUpdateStmt, 4,  item->updateStatus?1:0);
		sqlite3_bind_int (itemUpdateStmt, 5,  item->flagStatus?1:0);
		sqlite3_bind_text(itemUpdateStmt, 6,  item->source, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(itemUpdateStmt, 7,  item->sourceId, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int (itemUpdateStmt, 8,  item->validGuid?1:0);
		sqlite3_bind_text(itemUpdateStmt, 9,  item->real_source_url, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(itemUpdateStmt, 10, item->real_source_title, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(itemUpdateStmt, 11, item->description, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int (itemUpdateStmt, 12, item->time);
		sqlite3_bind_int (itemUpdateStmt, 13, item->id);
		res = sqlite3_step(itemUpdateStmt);

		if(SQLITE_DONE != res) 
			g_warning("item update failed (error code=%d, %s)", res, sqlite3_errmsg(db));
	} else {
		/* If it did not work or had no id insert it... */
		db_set_item_id(item);
		db_insert_item(item);
	}
}

void db_update_itemset(itemSetPtr itemSet) {
	GList	*iter;

	debug1(DEBUG_DB, "update of itemset for node \"%s\"", itemSet->node?node_get_title(itemSet->node):"null");
	iter = itemSet->items;
	while(iter) {
		db_update_item(iter->data);
		iter = g_list_next(iter);
	}
}

void db_remove_item_with_id(const gchar *id) {
}

void db_remove_all_items_with_node_id(const gchar *id) {
}

/* Statistics interface */

guint db_get_unread_count_with_node_id(const gchar *id) {

	return 5;
}

