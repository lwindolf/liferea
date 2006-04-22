#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "common.h"
#include "debug.h"
#include "node.h"
#include "support.h"
#include "update.h"
#include "feedlist.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_mainwindow.h"
#include "fl_providers/fl_plugin.h"
#include "fl_providers/fl_opml-cb.h"
#include "fl_providers/fl_opml-ui.h"

extern GtkWidget *mainwindow;

extern flPluginPtr fl_plugin_get_info();

static void fl_opml_initial_download_cb(struct request *request) {
	flPluginPtr	plugin;
	nodePtr		np = (nodePtr)request->user_data;
	gchar		*filename;

	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "plugins", np->id, "opml");
	debug2(DEBUG_UPDATE, "initial OPML download finished (%s) data=%d", filename, request->data);
	g_file_set_contents(filename, request->data, -1, NULL);
	g_free(filename);
	
	plugin = fl_plugin_get_info();
	plugin->handler_import(np);
}

static void on_fl_opml_source_selected(GtkDialog *dialog, gint response_id, gpointer user_data) {
	nodePtr		node;
	struct request	*request;
	const gchar	*source;

	if(response_id == GTK_RESPONSE_OK) {
		source = gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "location_entry")));

		/* add new node to feed list */
		node = node_new();
		node->icon = create_pixbuf("fl_opml.png");	// FIXME: correct place?
		node_set_title(node, _("New OPML Subscription"));
		node_add_data(node, FST_PLUGIN, NULL);
		node_add_child(NULL, node, 0);

		/* initial download */
		request = download_request_new();
		request->source = g_strdup(source);
		request->priority = 1;
		request->callback = fl_opml_initial_download_cb;
		request->user_data = node;
		debug1(DEBUG_UPDATE, "starting initial OPML download (%s.opml)", node->id);
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

static void on_file_select_clicked(const gchar *filename, gpointer user_data) {
	GtkWidget	*dialog = GTK_WIDGET(user_data);

	gtk_entry_set_text(GTK_ENTRY(lookup_widget(dialog, "location_entry")), g_strdup(filename));
}

void on_select_button_clicked(GtkButton *button, gpointer user_data) {

	ui_choose_file(_("Choose OPML File"), GTK_WINDOW(mainwindow), GTK_STOCK_OPEN, FALSE, on_file_select_clicked, NULL, NULL, user_data);
}

