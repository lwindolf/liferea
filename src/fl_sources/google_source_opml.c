
#include <glib.h>
#include <string.h>
#include <libxml/xpath.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "xml.h"

#include "google_source.h"
#include "subscription.h"
#include "node.h"

/**
 * Add "broadcast-friends" to the list of subscriptions if required 
 */
static void
google_source_add_broadcast_subscription (GoogleSourcePtr gsource)
{
	gchar* title = "Friend's Shared Items"; 
	GSList * iter = NULL ; 
	nodePtr node ; 

	iter = gsource->root->children; 
	while (iter) { 
		node = (nodePtr)iter->data ; 
		if (!node->subscription || !node->subscription->source) 
			continue;
		if (g_str_equal (node->subscription->source, GOOGLE_READER_BROADCAST_FRIENDS_URL)) {
			update_state_set_cookies (node->subscription->updateState, 
			                          gsource->sid);
			return;
		}
		iter = g_slist_next (iter);
	}

	/* aha! add it! */

	node = node_new ();
	node_set_title (node, title);
	node_set_type (node, feed_get_node_type ());
	node_set_data (node, feed_new ());

	node_set_subscription (node, subscription_new (GOOGLE_READER_BROADCAST_FRIENDS_URL, NULL, NULL));
	node->subscription->type = &googleSourceFeedSubscriptionType;
	node_set_parent (node, gsource->root, -1);
	feedlist_node_imported (node);
	
	update_state_set_cookies (node->subscription->updateState, gsource->sid);
	subscription_update (node->subscription, FEED_REQ_RESET_TITLE);
	subscription_update_favicon (node->subscription);
}



/* subscription list merging functions */

static void
google_source_check_for_removal (nodePtr node, gpointer user_data)
{
	gchar		*expr = NULL;

	if ( g_str_equal(node->subscription->source, GOOGLE_READER_BROADCAST_FRIENDS_URL) ) 
		return ; 
	if (IS_FEED (node)) {
		expr = g_strdup_printf ("/object/list[@name='subscriptions']/object/string[@name='id'][. = 'feed/%s']", node->subscription->source);
	} else {
		g_warning("opml_source_check_for_removal(): This should never happen...");
		return;
	}
	
	if (!xpath_find ((xmlNodePtr)user_data, expr)) {
		debug1 (DEBUG_UPDATE, "removing %s...", node_get_title (node));
		feedlist_node_removed (node);
	} else {
		debug1 (DEBUG_UPDATE, "keeping %s...", node_get_title (node));
	}
	g_free (expr);
}


static void
google_source_merge_feed (xmlNodePtr match, gpointer user_data)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr)user_data;
	nodePtr		node;
	GSList		*iter;
	xmlNodePtr	xml;
	xmlChar		*title, *id ;
	gchar           *url ;

	xml = xpath_find (match, "./string[@name='title']");
	if (xml)
		title = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
		
	xml = xpath_find (match, "./string[@name='id']");
	if (xml)
		id = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
	url = g_strdup(id+strlen("feed/"));

	/* check if node to be merged already exists */
	iter = gsource->root->children;
	while (iter) {
		node = (nodePtr)iter->data;
		if (g_str_equal (node->subscription->source, url)) {
			update_state_set_cookies (node->subscription->updateState, gsource->sid);
			node->subscription->type = &googleSourceFeedSubscriptionType;
			goto cleanup ;
		}
		iter = g_slist_next (iter);
	}
	
	/* Note: ids look like "feed/http://rss.slashdot.org" */
	if (id && title) {

		debug2 (DEBUG_UPDATE, "adding %s (%s)", title, url);
		node = node_new ();
		node_set_title (node, title);
		node_set_type (node, feed_get_node_type ());
		node_set_data (node, feed_new ());
		
		node_set_subscription (node, subscription_new (url, NULL, NULL));
		node->subscription->type = &googleSourceFeedSubscriptionType;
		node_set_parent (node, gsource->root, -1);
		feedlist_node_imported (node);
		
		update_state_set_cookies (node->subscription->updateState, gsource->sid);
		/**
		 * @todo mark the ones as read immediately after this is done
		 * the feed as retrieved by this has the read and unread
		 * status inherently.
		 */
		subscription_update (node->subscription,  FEED_REQ_RESET_TITLE);
		subscription_update_favicon (node->subscription);
	} else 
		g_warning("Unable to parse subscription information from Google");

cleanup:
	if (id)
		xmlFree (id);
	if (title)
		xmlFree (title);
	g_free (url) ;
}


/* OPML subscription type implementation */

static void
google_subscription_opml_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr)subscription->node->data;
	
	if (result->data) {
		xmlDocPtr doc = xml_parse (result->data, result->size, FALSE, NULL);
		if(doc) {		
			xmlNodePtr root = xmlDocGetRootElement (doc);
			
			/* Go through all existing nodes and remove those whose
			   URLs are not in new feed list. Also removes those URLs
			   from the list that have corresponding existing nodes. */
			node_foreach_child_data (subscription->node, google_source_check_for_removal, (gpointer)root);
			node_foreach_child (subscription->node, google_source_migrate_node);
						
			opml_source_export (subscription->node);	/* save new feed list tree to disk 
									   to ensure correct document in 
									   next step */

			xpath_foreach_match (root, "/object/list[@name='subscriptions']/object",
			                     google_source_merge_feed,
			                     (gpointer)gsource);
			google_source_add_broadcast_subscription (gsource) ;

			opml_source_export (subscription->node);	/* save new feeds to feed list */
						   
			subscription->node->available = TRUE;
			xmlFreeDoc (doc);
		} else { 
			/** @todo The session seems to have expired */
			g_warning("Unable to parse OPML list from google, the session might have expired.\n");
		}
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "google_subscription_opml_cb(): ERROR: failed to get subscription list!\n");
	}

	if (!(flags & GOOGLE_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));

}

static void
google_opml_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr)subscription->node->data;
	
	google_subscription_opml_cb (subscription, result, flags);
}

static gboolean
google_opml_subscription_prepare_update_request (subscriptionPtr subscription, struct updateRequest *request)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr)subscription->node->data;
	
	g_assert(gsource);
	if (gsource->loginState == GOOGLE_SOURCE_STATE_NONE) {
		debug0(DEBUG_UPDATE, "GoogleSource: login");
		google_source_login((GoogleSourcePtr)subscription->node->data, 0) ;
		return FALSE;
	}
	debug1 (DEBUG_UPDATE, "updating Google Reader subscription (node id %s)", subscription->node->id);
	
	update_request_set_source(request, GOOGLE_READER_SUBSCRIPTION_LIST_URL);
	
	update_state_set_cookies (request->updateState, gsource->sid);
	
	return TRUE;
}

/* OPML subscription type definition */

struct subscriptionType googleSourceOpmlSubscriptionType = {
	google_opml_subscription_prepare_update_request,
	google_opml_subscription_process_update_result,
	NULL	/* free */
};
