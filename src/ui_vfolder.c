/**
 * @file ui_vfolder.h  vfolder dialogs handling
 * 
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include "feed.h"
#include "rule.h"
#include "ui_vfolder.h"
#include "interface.h"
#include "support.h"

/********************************************************************
 * Propdialog                                                       *
 *******************************************************************/

struct fp_vfolder_ui_data {
	feedPtr		fp;
	
	GtkWidget	*dialog;
	GtkWidget	*feedNameEntry;
	GtkWidget	*ruleVBox;
};

struct changeRequest {
	GtkWidget			*hbox;		/* used for remove button */
	struct fp_vfolder_ui_data 	*ui_data;	/* used for remove button */ 
	gint				rule;		/* used for rule type change */
	GtkWidget			*paramHBox;	/* used for rule type change */
};

static void on_propdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);
static void on_addrulebtn_clicked(GtkButton *button, gpointer user_data);
static void on_ruletype_changed(GtkOptionMenu *optionmenu, gpointer user_data);
static void on_ruleremove_clicked(GtkButton *button, gpointer user_data);

GtkWidget* ui_vfolder_propdialog_new(GtkWindow *parent, feedPtr fp) {
	GtkWidget			*vfolderdialog;
	struct fp_vfolder_ui_data	*ui_data;

	ui_data = g_new0(struct fp_vfolder_ui_data, 1);
	ui_data->fp = fp;
	
	/* Create the dialog */
	ui_data->dialog = vfolderdialog = create_vfolderdialog();
	gtk_window_set_transient_for(GTK_WINDOW(vfolderdialog), GTK_WINDOW(parent));

	/* Setup feed name */
	ui_data->feedNameEntry = lookup_widget(vfolderdialog,"feedNameEntry");
	gtk_entry_set_text(GTK_ENTRY(ui_data->feedNameEntry), feed_get_title(fp));
	
	/* Set up rule list vbox */
	ui_data->ruleVBox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(lookup_widget(vfolderdialog, "ruleview")), ui_data->ruleVBox);
	gtk_widget_show_all(vfolderdialog);
	
	/* bind buttons */
	g_signal_connect(lookup_widget(vfolderdialog, "addrulebtn"), "clicked", G_CALLBACK(on_addrulebtn_clicked), ui_data);
	g_signal_connect(G_OBJECT(vfolderdialog), "response", G_CALLBACK(on_propdialog_response), ui_data);
	
	return vfolderdialog;
}

static void on_addrulebtn_clicked(GtkButton *button, gpointer user_data) {
	struct fp_vfolder_ui_data *ui_data = (struct fp_vfolder_ui_data*)user_data;
	GtkWidget		*hbox, *hbox2, *menu, *widget;
	struct changeRequest	*first, *changeRequest;
	ruleInfoPtr		ruleInfo;
	gint			i;
		
	/* this callback is used to add another rule to a vfolder dialog */

	hbox = gtk_hbox_new(FALSE, 2);	/* hbox to contain all rule widgets */
	hbox2 = gtk_hbox_new(FALSE, 2);	/* another hbox where the rule specific widgets are added */
		
	/* set up the rule type selection popup */
	menu = gtk_menu_new();
	for(i = 0, ruleInfo = ruleFunctions; i < nrOfRuleFunctions; i++, ruleInfo++) {
	
		/* we add a change request to each popup option */
		changeRequest = g_new0(struct changeRequest, 1);
		changeRequest->paramHBox = hbox2;
		changeRequest->rule = i;
		if(i == 0) 
			first = changeRequest;

		/* build the menu option */
		widget = gtk_menu_item_new_with_label(ruleInfo->title);
		gtk_widget_show(widget);
		gtk_container_add(GTK_CONTAINER(menu), widget);
		gtk_signal_connect(GTK_OBJECT(widget), "activate", GTK_SIGNAL_FUNC(on_ruletype_changed), changeRequest);
	}
	widget = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(widget), menu);	
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);
	
	/* fake a rule type change to set up parameter widgets */
	on_ruletype_changed(GTK_OPTION_MENU(widget), first);
	
	/* add remove button */
	changeRequest = g_new0(struct changeRequest, 1);
	changeRequest->hbox = hbox;
	changeRequest->ui_data = ui_data;
	widget = gtk_button_new_from_stock("gtk-remove");
	gtk_box_pack_end(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(widget), "clicked", GTK_SIGNAL_FUNC(on_ruleremove_clicked), changeRequest);

	/* and insert everything in the dialog */
	gtk_widget_show_all(hbox);
	gtk_box_pack_start(GTK_BOX(ui_data->ruleVBox), hbox, FALSE, TRUE, 0);
	
}

static void on_propdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_vfolder_ui_data *ui_data = (struct fp_vfolder_ui_data*)user_data;
	
	if(response_id == GTK_RESPONSE_OK) {
		// FIXME!!!
	}
	g_free(ui_data);
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void ui_vfolder_destroy_param_widget(GtkWidget *widget, gpointer data) {
	
	gtk_widget_destroy(widget);
}

static void on_ruletype_changed(GtkOptionMenu *optionmenu, gpointer user_data) {
	struct changeRequest	*changeRequest = (struct changeRequest *)user_data;
	ruleInfoPtr		ruleInfo;
	GtkWidget		*widget;
	GList			*iter;

	/* remove old widgets */	
	gtk_container_foreach(GTK_CONTAINER(changeRequest->paramHBox), ui_vfolder_destroy_param_widget, NULL);
		
	/* add new ones... */
	ruleInfo = ruleFunctions + changeRequest->rule;
	if(ruleInfo->needsParameter) {
		widget = gtk_entry_new();
		gtk_widget_show(widget);
		gtk_box_pack_start(GTK_BOX(changeRequest->paramHBox), widget, FALSE, FALSE, 0);
	} else {
		/* nothing needs to be added */
	}
}

static void on_ruleremove_clicked(GtkButton *button, gpointer user_data) {
	struct changeRequest *changeRequest = (struct changeRequest *)user_data;
	
	gtk_container_remove(GTK_CONTAINER(changeRequest->ui_data->ruleVBox), changeRequest->hbox);
	g_free(changeRequest);
}
