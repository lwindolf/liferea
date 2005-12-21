#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "common.h"
#include "node.h"
#include "support.h"
#include "update.h"
#include "fl_providers/fl_plugin.h"
#include "fl_providers/fl_opml-cb.h"
#include "fl_providers/fl_opml-ui.h"

static void fl_opml_initial_download_cb(struct request *request) {
	nodePtr		np = (nodePtr)request->user_data;
	gchar		*filename;
 	
	filename = common_create_cache_filename("plugins", np->id, "opml");
	g_file_set_contents(filename, request->data, -1, NULL);
	g_free(filename);
	fl_opml_handler_initial_load(np);

}

static void on_fl_opml_source_selected(GtkDialog *dialog, gint response_id, gpointer user_data) {
	nodePtr		np = (nodePtr)user_data;
	struct request	*request;
	const gchar	*source;

	if(response_id == GTK_RESPONSE_OK) {
		source = gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "location_entry")));

		/* initial download */
		request = download_request_new();
		request->source = g_strdup(source);
		request->priority = 1;
		request->callback = fl_opml_initial_download_cb;
		request->user_data = np;
		download_queue(request);
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
}

gchar * ui_fl_opml_get_handler_source(nodePtr np) {
	GtkWidget	*dialog;

	dialog = create_instance_dialog();

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(on_fl_opml_source_selected), 
			 (gpointer)np);

	gtk_widget_show_all(dialog);
}
