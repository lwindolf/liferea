/*
   filter GUI dialogs
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "callbacks.h"
#include "interface.h"
#include "feed.h"
#include "filter.h"
#include "support.h"

#define RS_TITLE	0
#define RS_VALUE	1
#define RS_PTR		2

extern feedPtr		selected_fp;
extern itemPtr		selected_ip;
extern gchar 		*selected_keyprefix;
extern gint		selected_type;

extern GtkWidget 	*mainwindow;
static GtkWidget 	*filterdialog = NULL;
static GtkTreeStore	*ruleStore = NULL;

static void setupRuleList(GtkWidget *ruleList) {
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;

	if(NULL == ruleStore) {
		/* set up a store of these attributes: 
			- rule type title
			- rule value
			- rule ptr
		 */
		ruleStore = gtk_tree_store_new(3, G_TYPE_STRING,
						  G_TYPE_STRING,
						  G_TYPE_POINTER);
	}
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(ruleList), GTK_TREE_MODEL(ruleStore));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Rule Type"), renderer, "text", RS_TITLE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ruleList), column);
	
	column = gtk_tree_view_column_new_with_attributes(_("Value"), renderer, "text", RS_VALUE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ruleList), column);
}

void on_popup_filter_selected(void) {
	GtkWidget	*ruleList;
	GtkTreeIter	iter;
	GSList		*rule;
	rulePtr		r;

	if(NULL == filterdialog || !G_IS_OBJECT(filterdialog))
		filterdialog = create_filterdialog();
	
	g_assert(NULL != filterdialog);
	
	if(NULL != (ruleList = lookup_widget(filterdialog, "rulelist"))) {
		setupRuleList(ruleList);
		gtk_tree_store_clear(GTK_TREE_STORE(ruleStore));
		
		g_assert(NULL != selected_fp);
		rule = selected_fp->filter;
		while(NULL != rule) {
			r = rule->data;
			gtk_tree_store_append(ruleStore, &iter, NULL);
			gtk_tree_store_set(ruleStore, &iter,
					   RS_TITLE, getRuleTitle(r),
					   RS_PTR, (gpointer)r,
					   -1);
			rule = g_slist_next(rule);
		}
		
		gtk_widget_show(filterdialog);
	}
}

/*------------------------------------------------------------------------------*/
/* filter edit dialog callbacks							*/
/*------------------------------------------------------------------------------*/

static gboolean getSelectedRuleIter(GtkTreeIter *iter) {
/*	GtkWidget		*treeview;
	GtkTreeSelection	*select;
        GtkTreeModel		*model;
	gchar			*tmp_key;
	gint			tmp_type;
	feedPtr			fp;
	
	if(NULL == filterdialog)
		return FALSE;
	
	if(NULL == (treeview = lookup_widget(mainwindow, "rulelist"))) {
		g_warning("rule list widget lookup failed!\n");
		return FALSE;
	}
		
	if(NULL == (select = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)))) {
		print_status("could not retrieve selection of entry list!");
		return FALSE;
	}

        gtk_tree_selection_get_selected(select, &model, iter);
	gtk_tree_model_get(GTK_TREE_MODEL(feedstore), iter, 
			   FS_KEY, &tmp_key, 
 			   FS_TYPE, &tmp_type,
			   -1);
	*/
	return TRUE;
}

/* adding a rule */
void on_addrulebtn_clicked(GtkButton *button, gpointer user_data) {
	
/*	if(NULL == ruledialog || !G_IS_OBJECT(ruledialog))
		ruledialog = create_ruledialog();
	
	g_assert(NULL != ruledialog);
	editedRule = NULL;
	gtk_widget_show(ruledialog);*/
}


/* editing a rule */
void on_rulepropbtn_clicked(GtkButton *button, gpointer user_data) {

/*	if(NULL == ruledialog || !G_IS_OBJECT(ruledialog))
		ruledialog = create_ruledialog();
	
	g_assert(NULL != ruledialog);
	editedRule = ...;
	gtk_widget_show(ruledialog);*/
}

/* called after finishing a add rule or edit rule dialog */
void on_rulechangedbtn_clicked(GtkButton *button, gpointer user_data) {
}

void on_ruleupbtn_clicked(GtkButton *button, gpointer user_data) {

}


void on_ruledownbtn_clicked(GtkButton *button, gpointer user_data) {

}
