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

static void ui_fl_opml_add(nodePtr np) {
	nodePtr		parent;
	int		pos;

	debug_enter("fl_opml_add");

	/* add new node to feed list */
	np->icon = create_pixbuf("fl_opml.png");
	node_set_title(np, _("New OPML Subscription"));
	parent = ui_feedlist_get_target_folder(&pos);
	feedlist_add_node(parent, np, pos);

	debug_exit("fl_opml_add");
}

static void fl_opml_initial_download_cb(struct request *request) {
	nodePtr		np = (nodePtr)request->user_data;
	gchar		*filename;

	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "plugins", np->id, "opml");
	debug2(DEBUG_UPDATE, "initial OPML download finished (%s) data=%d", filename, request->data);
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
		debug1(DEBUG_UPDATE, "starting initial OPML download (%s.opml)", np->id);
		download_queue(request);

		ui_fl_opml_add(np);
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

