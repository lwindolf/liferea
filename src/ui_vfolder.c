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
#include "vfolder.h"
#include "ui_itemlist.h"
#include "interface.h"
#include "support.h"

struct fp_vfolder_ui_data {
	feedPtr		fp;
	
	GtkWidget	*dialog;
	GtkWidget	*feedNameEntry;		/* widget with vfolder title */
	GtkWidget	*ruleVBox;		/* vbox containing the rule hboxes */
	GSList		*newRules;		/* new list of rules currently in editing */
};

struct changeRequest {
	GtkWidget			*hbox;		/* used for remove button */
	struct fp_vfolder_ui_data 	*ui_data;	/* used for both types */ 
	gint				rule;		/* used for rule type change */
	GtkWidget			*paramHBox;	/* used for rule type change */
};

static void on_propdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct fp_vfolder_ui_data	*ui_data = (struct fp_vfolder_ui_data*)user_data;
	GSList				*iter, *unused_rules;
	
	if(response_id == GTK_RESPONSE_OK) {
		feed_set_title(ui_data->fp, gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "feedNameEntry"))));
		unused_rules = ui_data->fp->rules;
		ui_data->fp->rules = ui_data->newRules;
	} else {
		unused_rules = ui_data->newRules;
	}

	/* delete old or unused rules */	
	iter = unused_rules;
	while(iter != NULL) {
		vfolder_remove_rule(ui_data->fp, (rulePtr)iter->data);
		rule_free((rulePtr)iter->data);
		iter = g_slist_next(iter);
	}
	g_slist_free(unused_rules);

	if(response_id == GTK_RESPONSE_OK) {	
		/* update vfolder */
		ui_itemlist_clear();
		vfolder_refresh(ui_data->fp);
		itemlist_load((nodePtr)ui_data->fp);
	}
	
	g_free(ui_data);
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void ui_vfolder_destroy_param_widget(GtkWidget *widget, gpointer data) {
	
	gtk_widget_destroy(widget);
}

static void on_rulevalue_changed(GtkEditable *editable, gpointer user_data) {
	rulePtr		rp = (rulePtr)user_data;
	
	rp->value = g_strdup(gtk_editable_get_chars(editable,0,-1));
}

/* callback for rule additive option menu */
static void on_rule_set_additive(GtkOptionMenu *optionmenu, gpointer user_data) {
	rulePtr		rp = (rulePtr)user_data;
	
	rp->additive = TRUE;
}

/* callback for rule additive option menu */
static void on_rule_unset_additive(GtkOptionMenu *optionmenu, gpointer user_data) {
	rulePtr		rp = (rulePtr)user_data;
	
	rp->additive = FALSE;
}

/* sets up the widgets for a single rule */
static void ui_vfolder_setup_rule_widgets(struct changeRequest *changeRequest, rulePtr rp) {
	GtkWidget		*widget, *menu;
	ruleInfoPtr		ruleInfo;

	ruleInfo = ruleFunctions + changeRequest->rule;
	g_object_set_data(G_OBJECT(changeRequest->paramHBox), "rule", rp);
			
	/* remove of old widgets */	
	gtk_container_foreach(GTK_CONTAINER(changeRequest->paramHBox), ui_vfolder_destroy_param_widget, NULL);

	/* add popup menu for selection of positive or negative logic */
	menu = gtk_menu_new();
	
	widget = gtk_menu_item_new_with_label(ruleInfo->positive);
	gtk_container_add(GTK_CONTAINER(menu), widget);
	gtk_signal_connect(GTK_OBJECT(widget), "activate", GTK_SIGNAL_FUNC(on_rule_set_additive), rp);
	
	widget = gtk_menu_item_new_with_label(ruleInfo->negative);
	gtk_container_add(GTK_CONTAINER(menu), widget);
	gtk_signal_connect(GTK_OBJECT(widget), "activate", GTK_SIGNAL_FUNC(on_rule_unset_additive), rp);
	
	gtk_menu_set_active(GTK_MENU(menu), (rp->additive)?0:1);
	
	widget = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(widget), menu);	
	gtk_widget_show_all(widget);
	gtk_box_pack_start(GTK_BOX(changeRequest->paramHBox), widget, FALSE, FALSE, 0);
		
	/* add new value entry if needed */
	if(ruleInfo->needsParameter) {
		widget = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(widget), rp->value);
		gtk_widget_show(widget);
		gtk_signal_connect(GTK_OBJECT(widget), "changed", GTK_SIGNAL_FUNC(on_rulevalue_changed), rp);
		gtk_box_pack_start(GTK_BOX(changeRequest->paramHBox), widget, FALSE, FALSE, 0);
	} else {
		/* nothing needs to be added */
	}
}

/* callback for rule type option menu */
static void on_ruletype_changed(GtkOptionMenu *optionmenu, gpointer user_data) {
	struct changeRequest	*changeRequest = (struct changeRequest *)user_data;
	ruleInfoPtr		ruleInfo;
	rulePtr			rule;
	
	if(NULL != (rule = g_object_get_data(G_OBJECT(changeRequest->paramHBox), "rule"))) {
		changeRequest->ui_data->newRules = g_slist_remove(changeRequest->ui_data->newRules, rule);
		rule_free(rule);
	}
	ruleInfo = ruleFunctions + changeRequest->rule;
	rule = rule_new(changeRequest->ui_data->fp, ruleInfo->ruleId, "", TRUE);
	changeRequest->ui_data->newRules = g_slist_append(changeRequest->ui_data->newRules, rule);
	
	ui_vfolder_setup_rule_widgets(changeRequest, rule);
}

/* callback for each rules remove button */
static void on_ruleremove_clicked(GtkButton *button, gpointer user_data) {
	struct changeRequest	*changeRequest = (struct changeRequest *)user_data;
	rulePtr			rule;
	
	if(NULL != (rule = g_object_get_data(G_OBJECT(changeRequest->paramHBox), "rule"))) {
		changeRequest->ui_data->newRules = g_slist_remove(changeRequest->ui_data->newRules, rule);
		rule_free(rule);
	}
	gtk_container_remove(GTK_CONTAINER(changeRequest->ui_data->ruleVBox), changeRequest->hbox);
	g_free(changeRequest);
}

/**
 * This method is used to add another rule to a vfolder dialog. The rule
 * parameter might be NULL to create new rules or a pointer to an 
 * existing rule to load it into the dialog.
 */
static void ui_vfolder_add_rule(struct fp_vfolder_ui_data *ui_data, rulePtr rule) {
	GtkWidget		*hbox, *hbox2, *menu, *widget;
	struct changeRequest	*changeRequest, *selected = NULL;
	ruleInfoPtr		ruleInfo;
	rulePtr			rp;
	gint			i;

	hbox = gtk_hbox_new(FALSE, 2);	/* hbox to contain all rule widgets */
	hbox2 = gtk_hbox_new(FALSE, 2);	/* another hbox where the rule specific widgets are added */
		
	/* set up the rule type selection popup */
	menu = gtk_menu_new();
	for(i = 0, ruleInfo = ruleFunctions; i < nrOfRuleFunctions; i++, ruleInfo++) {
	
		/* we add a change request to each popup option */
		changeRequest = g_new0(struct changeRequest, 1);
		changeRequest->paramHBox = hbox2;
		changeRequest->rule = i;
		changeRequest->ui_data = ui_data;
		
		if(0 == i)
			selected = changeRequest;
		
		/* build the menu option */
		widget = gtk_menu_item_new_with_label(ruleInfo->title);
		gtk_container_add(GTK_CONTAINER(menu), widget);
		gtk_signal_connect(GTK_OBJECT(widget), "activate", GTK_SIGNAL_FUNC(on_ruletype_changed), changeRequest);

		if(NULL != rule) {
			if(ruleInfo == rule->ruleInfo) {
				selected = changeRequest;
				gtk_menu_set_active(GTK_MENU(menu), i);
			}
		}
	}
	widget = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(widget), menu);	
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);
	
	if(NULL == rule) {
		/* fake a rule type change to initialize parameter widgets */
		on_ruletype_changed(GTK_OPTION_MENU(widget), selected);
	} else {
		/* set up widgets with existing rule type and value */
		ui_vfolder_setup_rule_widgets(selected, rp = rule_new(rule->fp, rule->ruleInfo->ruleId, rule->value, rule->additive));
		/* add the rule to the list of new rules */
		ui_data->newRules = g_slist_append(ui_data->newRules, rp);
	}
	
	/* add remove button */
	changeRequest = g_new0(struct changeRequest, 1);
	changeRequest->hbox = hbox;
	changeRequest->paramHBox = hbox2;
	changeRequest->ui_data = ui_data;
	widget = gtk_button_new_from_stock("gtk-remove");
	gtk_box_pack_end(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(widget), "clicked", GTK_SIGNAL_FUNC(on_ruleremove_clicked), changeRequest);

	/* and insert everything in the dialog */
	gtk_widget_show_all(hbox);
	gtk_box_pack_start(GTK_BOX(ui_data->ruleVBox), hbox, FALSE, TRUE, 0);
}

static void on_addrulebtn_clicked(GtkButton *button, gpointer user_data) {
	struct fp_vfolder_ui_data *ui_data = (struct fp_vfolder_ui_data*)user_data;
		
	ui_vfolder_add_rule(ui_data, NULL);
}

GtkWidget* ui_vfolder_propdialog_new(GtkWindow *parent, feedPtr fp) {
	GtkWidget			*vfolderdialog;
	GSList				*iter;
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
	
	/* load rules into dialog */	
	iter = fp->rules;
	while(NULL != iter) {
		ui_vfolder_add_rule(ui_data, (rulePtr)(iter->data));
		iter = g_slist_next(iter);
	}
	
	/* bind buttons */
	g_signal_connect(lookup_widget(vfolderdialog, "addrulebtn"), "clicked", G_CALLBACK(on_addrulebtn_clicked), ui_data);
	g_signal_connect(G_OBJECT(vfolderdialog), "response", G_CALLBACK(on_propdialog_response), ui_data);
	
	gtk_widget_show_all(vfolderdialog);	
	
	return vfolderdialog;
}
