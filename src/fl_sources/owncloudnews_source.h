#ifndef _OWNCLOUDNEWS_SOURCE_H
#define _OWNCLOUDNEWS_SOURCE_H

#include <glib.h>

#include "fl_sources/node_source.h"
#include "node.h"
#include "update.h"

// A nodeSource specific for ownCloud News
typedef struct owncloudnewsSource {
	// the root node in the feed list
	nodePtr	root;
} *owncloudnewsSourcePtr;

/**
 * @brief Source type initialization
 *
 * Register URL and feed ID metadata types.
 *
 * @param  void
 * @return void
 */
static void source_init (void);

/**
 * @brief Source type de-initialization
 *
 * No-op
 *
 * @param void
 * @return void
 */
static void source_deinit (void);

/**
 * @brief Render an account info dialog
 *
 * Show a dialog where the user enters the ownCloud username,
 * password, and server URL.
 *
 * @param  void
 * @return void
 */
static void show_account_info_dialog (void);

/**
 * @brief Handle the account info dialog response
 *
 * If the user credentials are good, create an ownCloud News source
 * and insert it into the UI.
 *
 * TODO: Handle invalid server URL, or show any server error message to the user.
 *
 * @param dialog GtkDialog used to get the user info
 * @param response_id User response
 * @param user_data User data
 * @return void
 */
static void process_account_info_dialog (
	GtkDialog *dialog, gint response_id, gpointer user_data);

/**
 * @brief create a new source and make `node` its root
 *
 * @param node nodePtr
 * @return owncloudnewsSourcePtr
 */
static owncloudnewsSourcePtr source_new (nodePtr node);

/**
 * @brief import the node into feed list
 *
 * Called when the source is to create the feed list subtree
 * attached to the source root node.
 *
 * @param node nodePtr
 * @return void
 */
static void source_import (nodePtr node);

/**
 * @brief auto update source
 *
 * Called to request the source to update its subscriptions
 * list and the child subscriptions according the its update interval.
 *
 * @param node nodePtr
 * @return void
 */
static void source_auto_update (nodePtr node);

/**
 * @brief free all data of the node
 *
 * @param node nodePtr
 * @return void
 */
static void source_free (nodePtr node);

/**
 * @brief mark remote flag status of the item
 *
 * If item flag status cannot be updated in the remote server,
 * mark it so locally.
 *
 * @param *result updateResult
 * @param userdata gpoionter
 * @param flags updateFlags
 * @return void
 */
static void remote_item_set_flag_cb (const struct updateResult * const result,
									 gpointer userdata, updateFlags flags);
/**
 * @brief Toggle the flag state of the item and sync
 *
 * @see https://github.com/owncloud/news/wiki/Items-1.2#mark-an-item-as-starred
 *
 * @param node nodePtr
 * @param item itemPtr
 * @param newState gboolean
 * @return void
 */
static void item_set_flag (nodePtr node, itemPtr item, gboolean newState);

/**
 * @brief mark item read/unread and sync
 *
 * @see https://github.com/owncloud/news/wiki/Items-1.2#mark-an-item-as-read
 *
 * @param node nodePtr
 * @param item itemPtr
 * @param newState gboolean
 * @return void
 */
static void item_mark_read (nodePtr node, itemPtr item, gboolean newState);

/**
 * @brief add a new folder to the feed list provided by node source
 *
 * @see https://github.com/owncloud/news/wiki/Folders-1.2#create-a-folder
 * @param node nodePtr
 * @param *title gchar
 * @return nodePtr
 */
nodePtr add_folder (nodePtr node, const gchar *title);

/**
 * TODO: document
 */
static void add_folder_cb (
	const struct updateResult * const result,
	gpointer userdata,
	updateFlags flags
);

/**
 * @brief Create a node folder using the API response
 *
 * @param *data gchar Something like: '{"folders": [{ "id": 1, "name": "GNOME"}]}'
 * @return gboolean TRUE if the folder(s) has/have been created successfully
 */
gboolean owncloudnews_source_create_folder_from_api_response (
	owncloudnewsSourcePtr source,
	gchar *data
);

/**
 * @brief add a new subscription to the feed list provided by the node source
 * TODO: implement
 *
 * @param node nodePtr
 * @param *subscription struct subscription
 * @return nodePtr
 */
nodePtr add_subscription (
	nodePtr node, struct subscription *subscription);

/**
 * @brief removes an existing node (subscription or folder) from the feed list
 * provided by the node source
 * TODO: implement
 *
 * @param node nodePtr
 * @param child nodePtr
 * @return void
 */
static void remove_node (nodePtr node, nodePtr child);

/**
 * @brief Remove node callback
 *
 * @param *result updateResult
 * @param userdata gpointer
 * @param flags updateFlags
 * @return void
 */
static void remove_node_cb (
	const struct updateResult * const result,
	gpointer userdata,
	updateFlags flags
);

/**
 * @brief convert all subscriptions to default source subscriptions
 * TODO: implement
 *
 * @param node nodePtr
 * @return void
 */
static void convert_to_local (nodePtr node);

/**
 * @brief Log remote update status
 *
 * @param *result updateResult
 * @param userdata gpointer
 * @param flags updateFlags
 * @return void
 */
static void remote_update_cb(
	const struct updateResult * const result,
	gpointer userdata,
	updateFlags flags
);

/**
 * TODO: document well
 * @returns ownCloudNews source type implementation info.
 */
nodeSourceTypePtr owncloudnews_source_get_type (void);

/**
* API documentation is at https://github.com/owncloud/news/wiki/API-1.2
* TODO clean-up and use a function to return different URLs
*/
#define OWNCLOUDNEWS_URL "%s/index.php/apps/news/api/v1-2/"
#define OWNCLOUDNEWS_STATUS_URL "%s/index.php/apps/news/api/v1-2/status"
#define OWNCLOUDNEWS_FOLDERS_URL "%s/index.php/apps/news/api/v1-2/folders"
#define OWNCLOUDNEWS_FEEDS_URL "%s/index.php/apps/news/api/v1-2/feeds"
#define OWNCLOUDNEWS_ITEMS_URL "%s/index.php/apps/news/api/v1-2/items"

#endif //_OWNCLOUDNEWS_SOURCE_H


