#include "owncloudnews_source.h"

#include <glib.h>
#include <libsoup/soup-session.h>
#include <libsoup/soup.h>
#include <folder.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"
#include "json.h"
#include "item_state.h"
#include "metadata.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_common.h"
#include "update.h"

updateResultPtr owncloudnews_source_make_sync_request(updateRequestPtr request)
{
	// TODO: take care of DNT, etc. See network_process_request()
	updateResultPtr result = update_result_new();
	SoupSession *session = soup_session_new();
	SoupMessage *message;

	message = soup_message_new (
		request->method == "PUT" ?
			SOUP_METHOD_PUT :
			( request->postdata ? SOUP_METHOD_POST : SOUP_METHOD_GET ),
		request->source);

	if (request->authValue) {
		soup_message_headers_append (
			message->request_headers, "Authorization", request->authValue);
	}

	if (request->contentType) {
		soup_message_headers_append (
			message->request_headers, "Content-Type", request->contentType);
	}

	if (request->postdata) {
		soup_message_set_request (
			message,
			"application/x-www-form-urlencoded",
		    SOUP_MEMORY_COPY,
			request->postdata,
			strlen (request->postdata)
		);
	}


	result->httpstatus = soup_session_send_message (session, message);
	result->data = g_strdup_printf("%s", message->response_body->data);
	result->size = (size_t) message->response_body->length;
	return result;
}

static void source_init (void)
{
	metadata_type_register ("owncloudnews-auth-token", METADATA_TYPE_TEXT);
	metadata_type_register ("owncloudnews-feed-id", METADATA_TYPE_TEXT);
	metadata_type_register ("owncloudnews-item-id", METADATA_TYPE_TEXT);
}

static void source_deinit (void)
{
	// no-op
}

static void show_account_info_dialog (void)
{
	GtkWidget	*dialog;
	dialog = liferea_dialog_new ("owncloudnews_source.ui",
								 "owncloudnews_source_dialog");
	g_signal_connect (G_OBJECT (dialog), "response",
					  G_CALLBACK (process_account_info_dialog), NULL);
}

static void process_account_info_dialog (
	GtkDialog *dialog, gint response_id, gpointer user_data)
{
	// TODO: what if the server URL or credentials are invalid?
	if (response_id == GTK_RESPONSE_OK) {
		nodePtr node = node_new (node_source_get_node_type ());
		node_source_new (
			node,
			owncloudnews_source_get_type (),
			gtk_entry_get_text (GTK_ENTRY (
				liferea_dialog_lookup (GTK_WIDGET (dialog), "serverRootUrlEntry")))
		);
		subscription_set_auth_info (
			node->subscription,
			gtk_entry_get_text (GTK_ENTRY (
				liferea_dialog_lookup (GTK_WIDGET (dialog), "usernameEntry"))),
			gtk_entry_get_text (GTK_ENTRY (
				liferea_dialog_lookup (GTK_WIDGET (dialog), "passwordEntry")))
		);

		gchar *data, *encoded_data;
		data = g_strdup_printf (
			"%s:%s",
			node->subscription->updateOptions->username,
			node->subscription->updateOptions->password
		);
		encoded_data = g_base64_encode (
			(const guchar *)data, (gsize)g_utf8_strlen(data, 256)
		);
		metadata_list_set (
			&node->subscription->metadata,
			"owncloudnews-auth-token",
			g_strdup_printf("Basic %s", encoded_data)
		);
		g_free(data);
		g_free(encoded_data);

		node->data = (gpointer) source_new (node);
		feedlist_node_added (node);
		node_source_update (node);
		db_node_update (node);
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static owncloudnewsSourcePtr source_new (nodePtr node) {
	owncloudnewsSourcePtr source = g_new0 (struct owncloudnewsSource, 1) ;
	source->root = node;

	return source;
}

static void source_import (nodePtr node) {
	opml_source_import (node);
	node->subscription->updateInterval = -1;
	node->subscription->type = node->source->type->sourceSubscriptionType;
	if (!node->data)
		node->data = (gpointer) source_new (node);
}

static void source_auto_update (nodePtr node)
{
}

static void source_free (nodePtr node)
{
	owncloudnewsSourcePtr source = (owncloudnewsSourcePtr) node->data;
	if (source) {
		update_job_cancel_by_owner (source);
		g_free (source);
		node->data = NULL;
	}
}

static void item_set_flag (
	nodePtr node, itemPtr item, gboolean newState)
{
	nodePtr root = node_source_root_from_node (node);
	owncloudnewsSourcePtr source = (owncloudnewsSourcePtr)root->data;
	updateRequestPtr request = update_request_new ();
	gchar *source_uri = g_strdup_printf (
		OWNCLOUDNEWS_ITEMS_URL,
		source->root->subscription->source
	);
	gchar *query_parameter = g_strdup_printf (
		"%s/%s/%s",
		item->parentNodeId,
		item->sourceId,
		newState ? "star" : "unstar"
	);
	source_uri = g_strdup_printf ("%s/%s", source_uri, query_parameter);

	request->method = "PUT";
	request->contentType = "application/json";
	request->options = update_options_copy (node->subscription->updateOptions);
	update_request_set_source (request, source_uri);
	update_request_set_auth_value (
		request,
		metadata_list_get (source->root->subscription->metadata, "owncloudnews-auth-token")
	);
	g_free (source_uri);
	g_free (query_parameter);

	update_execute_request (source, request, remote_item_set_flag_cb, item, 0);
	item_flag_state_changed (item, newState);
}

// FIXME: use this remoteFlagStatus info when syncing the next time
static void remote_item_set_flag_cb (
	const struct updateResult * const result,
	gpointer userdata,
	updateFlags flags
) {
	debug2 (DEBUG_UPDATE, "ownCloudNews update result processing..."
		" status:%d >>>%s<<<", result->httpstatus, result->data);
	if (result->httpstatus != 200) {
		itemPtr item = (itemPtr)userdata;
		item->remoteFlagStatus = !item->flagStatus;
	}
}

// TODO: batch update
static void item_mark_read (
	nodePtr node, itemPtr item, gboolean newState)
{
	nodePtr root = node_source_root_from_node (node);
	owncloudnewsSourcePtr source = (owncloudnewsSourcePtr)root->data;
	updateRequestPtr request = update_request_new ();
	gchar *source_uri = g_strdup_printf (
		OWNCLOUDNEWS_ITEMS_URL,
		source->root->subscription->source
	);
	gchar *query_parameter = g_strdup_printf (
		"%s/%s",
		metadata_list_get( item->metadata, "owncloudnews-item-id" ),
		newState ? "read" : "unread"
	);

	source_uri = g_strdup_printf ("%s/%s", source_uri, query_parameter);
	request->method = "PUT";
	request->contentType = "application/json";
	request->options = update_options_copy (node->subscription->updateOptions);
	update_request_set_source (request, source_uri);
	update_request_set_auth_value (
		request,
		metadata_list_get (source->root->subscription->metadata, "owncloudnews-auth-token")
	);
	// TODO: if offline, sync later - save this info somewhere
	update_execute_request (source, request, remote_update_cb, source, 0);
	item_read_state_changed (item, newState);

	g_free (source_uri);
	g_free (query_parameter);
}

// TODO: the function must return nodePtr
// Should we make a sync request instead?
nodePtr add_folder (nodePtr node, const gchar *title)
{
	nodePtr folder;
	nodePtr root = node_source_root_from_node (node);
	owncloudnewsSourcePtr source = (owncloudnewsSourcePtr)root->data;
	updateRequestPtr request = update_request_new ();
	gchar *source_uri = g_strdup_printf (
		OWNCLOUDNEWS_FOLDERS_URL,
		source->root->subscription->source
	);
	request->contentType = "application/json";
	request->options = update_options_copy (node->subscription->updateOptions);
	update_request_set_source (request, source_uri);
	update_request_set_auth_value (
		request,
		metadata_list_get (source->root->subscription->metadata, "owncloudnews-auth-token")
	);
	request->postdata = soup_form_encode ("name", title, NULL);

	updateResultPtr result = owncloudnews_source_make_sync_request (request);
	if (result->httpstatus == 409) {  // Does the folder exist already?
		// TODO: handle
	} else if (result->httpstatus == 422) {  // Is the folder name invalid?
		// TODO: handle
	} else if (result->httpstatus == 200 && result->data) {  // All cool?
		if (owncloudnews_source_create_folder_from_api_response (
			source, result->data) ) {
			// TODO: parse JSON and create the folder node to return
		} else {
			// TODO: handle, something is wrong with the data
		}
	} else {  // TODO: show some error?
	}
	return folder;
}

gboolean owncloudnews_source_create_folder_from_api_response (
		owncloudnewsSourcePtr source, gchar *data)
{
	gboolean result;
	JsonParser *parser = json_parser_new();

	if (json_parser_load_from_data (parser, data, -1, NULL)) {
		JsonNode *folders = json_get_node (
			json_parser_get_root (parser), "folders");

		if (!folders || (JSON_NODE_TYPE (folders) != JSON_NODE_ARRAY)) {
			result = FALSE;
		} else {
			JsonArray *array;
			GList *iter, *elements;
			array = json_node_get_array (folders);
			elements = iter = json_array_get_elements (array);
			while (iter) {
				JsonNode *node = (JsonNode *) iter->data;
				gint folder_id = (gint) json_get_int(node, "id");
				gchar *folder_name = (gchar *) json_get_string (node, "name");
				gchar *folder_id_str = g_strdup_printf ("%d", folder_id);

				node_source_find_or_create_folder (
					source->root, folder_id_str, folder_name);

				iter = g_list_next (iter);
			}
			g_list_free (elements);
			result = TRUE;
		}
	}

	g_object_unref (parser);

	return result;
}

nodePtr add_subscription (nodePtr node, struct subscription *subscription) {}

static void remove_node (nodePtr node, nodePtr child) {
	owncloudnewsSourcePtr source = (owncloudnewsSourcePtr)node->data;
	updateRequestPtr request = update_request_new ();

	request->method = "DELETE";
	request->contentType = "application/json";
	request->options = update_options_copy (node->subscription->updateOptions);
	update_request_set_auth_value (
		request,
		metadata_list_get (source->root->subscription->metadata, "owncloudnews-auth-token")
	);

	if (IS_FOLDER (child)) {
		gchar *source_uri = g_strdup_printf (
			OWNCLOUDNEWS_FOLDERS_URL,
			source->root->subscription->source
		);
		// TODO: find a better way to retrieve the id. Maybe store it when creating the folder node?
		gchar **tokens = g_strsplit( child->id, "-", 3);
		source_uri = g_strdup_printf ("%s/%s", source_uri, tokens[2]);
		update_request_set_source (request, source_uri);
		g_free (source_uri);
	} else {
		// TODO: so we're removing a feed
	}
	update_execute_request (source, request, remove_node_cb, child, 0);
}

static void remove_node_cb (
	const struct updateResult * const result,
	gpointer userdata,
	updateFlags flags
) {
	if (result->httpstatus == 200) {
		feedlist_node_removed ((nodePtr)userdata);
	} else if (result->httpstatus == 400) {
		// Folder does not exist. TODO: show the error.
	} else {
		// TODO: show the error error
	}
}

static void convert_to_local (nodePtr node) {}

static void
remote_update_cb (
	const struct updateResult * const result,
	gpointer userdata,
	updateFlags flags
) {
	debug2 (DEBUG_UPDATE, "ownCloudNews update result processing..."
		" status:%d >>>%s<<<", result->httpstatus, result->data);
}

// Feed subscription type
extern struct subscriptionType owncloudnewsSourceFeedSubscriptionType;
// Source subscription type
extern struct subscriptionType owncloudnewsSourceSubscriptionType;

// Feed list node source type
static struct nodeSourceType owncloudnewsNodeSourceType = {
	.id = "fl_owncloudnews",
	.name = N_("ownCloud News"),
	.capabilities = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
		NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
		NODE_SOURCE_CAPABILITY_ADD_FEED |
		NODE_SOURCE_CAPABILITY_ADD_FOLDER |
		NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
		NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL,
	.feedSubscriptionType = &owncloudnewsSourceFeedSubscriptionType,
	.sourceSubscriptionType = &owncloudnewsSourceSubscriptionType,
	.source_type_init = source_init,
	.source_type_deinit = source_deinit,
	.source_new = show_account_info_dialog,
	.source_delete = opml_source_remove,
	.source_import = source_import,
	.source_export = opml_source_export,
	.source_get_feedlist = opml_source_get_feedlist,
	.source_auto_update = source_auto_update,
	.free = source_free,
	.item_set_flag = item_set_flag,
	.item_mark_read = item_mark_read,
	.add_folder = add_folder,
	.add_subscription = add_subscription,
	.remove_node = remove_node,
	.convert_to_local = convert_to_local
};

nodeSourceTypePtr owncloudnews_source_get_type (void)
{
	return &owncloudnewsNodeSourceType;
}
