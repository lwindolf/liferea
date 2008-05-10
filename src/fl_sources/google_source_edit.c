#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include "debug.h"
#include "update.h"
#include "subscription.h"
#include "common.h"


#include "google_source.h"
#include "google_source_edit.h"

editPtr 
google_source_edit_new (void)
{
	editPtr edit = g_new0 (struct edit, 1);
	return edit;
}

void 
google_source_edit_free (editPtr edit)
{ 
	g_free (edit->guid);
	g_free (edit->feedUrl);
	g_free (edit);
}

static void
google_source_edit_action_complete(
	const struct updateResult* const result, 
	gpointer userdata, 
	updateFlags flags) 
{ 
	readerPtr reader = (readerPtr) userdata;

	if ( result->data == NULL || !g_str_equal(result->data, "OK")) 
		debug1(DEBUG_UPDATE, "The edit action failed with result: %s\n",
		       result->data);
	
	/* process anything else waiting on the edit queue */
	google_source_edit_process (reader);
}

static void
google_source_api_edit_subscription(editPtr edit, updateRequestPtr request, const gchar* token) 
{
	update_request_set_source(request, "http://www.google.com/reader/api/0/subscription/edit?client=liferea");
	gchar* s_escaped = common_uri_escape(edit->feedUrl) ;
	gchar* postdata = g_strdup_printf("s=feed%%2F%s&ac=subscribe&token=%s",
					  s_escaped, token) ;
	g_free(s_escaped);
	
	request->postdata = postdata ;
}

static void 
google_source_api_edit_tag(editPtr edit, updateRequestPtr request, const gchar*token) 
{
	update_request_set_source(request, "http://www.google.com/reader/api/0/edit-tag?client=liferea"); 

	gchar* s_escaped = common_uri_escape (edit->feedUrl);
	gchar* a_escaped = NULL ;
	gchar* i_escaped = common_uri_escape (edit->guid);
	gchar* postdata = NULL ;

	if (edit->action == EDIT_ACTION_MARK_UNREAD) {
		a_escaped = common_uri_escape ("user/-/state/com.google/kept-unread");
		gchar *r_escaped = common_uri_escape ("user/-/state/com.google/read");
		postdata = g_strdup_printf ("i=%s&s=feed%%2F%s&a=%s&r=%s&ac=edit-tags&T=%s", i_escaped, s_escaped, a_escaped, r_escaped, token);
		g_free (r_escaped);
	}
	else if (edit->action == EDIT_ACTION_MARK_READ) { 
		a_escaped = common_uri_escape ("user/-/state/com.google/read");
		postdata = g_strdup_printf ("i=%s&s=feed%%2F%s&a=%s&ac=edit-tags&T=%s", i_escaped, s_escaped, a_escaped, token);
	}
	else if (edit->action == EDIT_ACTION_TRACKING_MARK_UNREAD) {
		a_escaped = common_uri_escape ("user/-/state/com.google/tracking-kept-unread");
		postdata = g_strdup_printf ("i=%s&s=feed%%2F%s&a=%s&ac=edit-tags&async=true&T=%s", i_escaped, s_escaped, a_escaped, token);
	}  else g_assert(FALSE);
	
	g_free (s_escaped);
	g_free (a_escaped); 
	g_free (i_escaped);
	
	debug1 (DEBUG_UPDATE, "google_source: postdata [%s]", postdata);


	request->postdata = postdata;
}

/**
 * Callback from an token request, and sends the actual edit request
 * in processing
 */
static void
google_source_edit_token_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{ 
	if (result->returncode != 0) { 
		/* What is the behaviour that should go here? */
		return;
	}

	readerPtr reader = (readerPtr) userdata; 
	const gchar* token = result->data; 

	if (g_queue_is_empty (reader->editQueue))
		return;

	editPtr edit = g_queue_peek_head (reader->editQueue);
	updateRequestPtr request = update_request_new ();
	request->updateState = update_state_copy (reader->root->subscription->updateState);
	request->options = update_options_copy (reader->root->subscription->updateOptions) ;
	update_state_set_cookies (request->updateState, reader->sid);

	if ( edit->action == EDIT_ACTION_MARK_READ || 
	     edit->action == EDIT_ACTION_MARK_UNREAD || 
	     edit->action == EDIT_ACTION_TRACKING_MARK_UNREAD ) 
	  google_source_api_edit_tag (edit, request, token);
	else if (edit->action == EDIT_ACTION_ADD_SUBSCRIPTION ) 
	  google_source_api_edit_subscription(edit, request, token);
		
	update_execute_request (reader, request, google_source_edit_action_complete, 
	                        reader, 0);

	edit = g_queue_pop_head (reader->editQueue);
	google_source_edit_free (edit) ;
}

void
google_source_edit_process (readerPtr reader)
{ 
	g_assert (reader);
	if (g_queue_is_empty (reader->editQueue))
		return;
	
	/*
 	* Google reader has a system of tokens. So first, I need to request a 
 	* token from google, before I can make the actual edit request. The
 	* code here is the token code, the actual edit commands are in 
 	* google_source_edit_token_cb
	 */
	updateRequestPtr request = update_request_new ();
	request->updateState = update_state_copy (reader->root->subscription->updateState);
	request->options = update_options_copy (reader->root->subscription->updateOptions);
	request->source = g_strdup ("http://www.google.com/reader/api/0/token");
	update_state_set_cookies (request->updateState, reader->sid);

	update_execute_request (reader, request, google_source_edit_token_cb, 
	                        reader, 0);
}

void
google_source_edit_push_ (readerPtr reader, editPtr edit)
{ 
	g_assert (reader->editQueue);
	g_queue_push_tail (reader->editQueue, edit) ;
}

void 
google_source_edit_push (readerPtr reader, editPtr edit)
{
	g_assert(reader);
	nodePtr root = reader->root ;

	/** @todo any flags I should specify? */
	if (reader->loginState == READER_STATE_NONE) {
		google_source_login (root->subscription, 0);
		subscription_update(root->subscription, 0) ;
		google_source_edit_push_ (reader, edit);
	} else if (reader->loginState == READER_STATE_IN_PROGRESS) {
		google_source_edit_push_ (reader, edit);
	} else { 
		google_source_edit_push_ (reader, edit);
		google_source_edit_process (reader);
	}
	
}


void
google_source_edit_mark_read (
	readerPtr reader, 
	const gchar *guid,
	const gchar *feedUrl,
	gboolean newStatus)
{
	editPtr edit = google_source_edit_new ();

	edit->guid = g_strdup (guid);
	edit->feedUrl = g_strdup (feedUrl);
	edit->action = newStatus ? EDIT_ACTION_MARK_READ :
	                           EDIT_ACTION_MARK_UNREAD;

	google_source_edit_push (reader, edit);

	if (newStatus == FALSE) { 
		/*
		 * According to the Google Reader API, to mark an item unread, 
		 * I also need to mark it as tracking-kept-unread in a separate
		 * network call.
		 */
		edit = google_source_edit_new ();
		edit->guid = g_strdup (guid);
		edit->feedUrl = g_strdup (feedUrl);
		edit->action = EDIT_ACTION_TRACKING_MARK_UNREAD;
		google_source_edit_push (reader, edit);
	}
}


void google_source_edit_add_subscription(
	readerPtr reader, 
	const gchar* feedUrl)
{
	editPtr edit = google_source_edit_new() ;
	edit->action = EDIT_ACTION_ADD_SUBSCRIPTION ; 
	edit->feedUrl = g_strdup(feedUrl) ;

	google_source_edit_push(reader, edit);
}

