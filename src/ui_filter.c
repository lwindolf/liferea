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
#define RS_PTR		1

extern feedPtr		selected_fp;
extern itemPtr		selected_ip;
extern gchar 		*selected_keyprefix;
extern gint		selected_type;

extern GSList		*allItems;

extern GtkWidget 	*mainwindow;
static GtkWidget 	*filterdialog = NULL;
static GtkWidget	*ruledialog = NULL;
static GtkTreeStore	*ruleStore = NULL;

static void setupRuleList(GtkWidget *ruleList) {
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;

	if(NULL == ruleStore) {
		/* set up a store of these attributes: 
			- rule title
			- rule ptr
		 */
		ruleStore = gtk_tree_store_new(2, G_TYPE_STRING, 
						  G_TYPE_POINTER);
	}
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(ruleList), GTK_TREE_MODEL(ruleStore));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Rule"), renderer, "text", RS_TITLE, NULL);
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

void
on_addrulebtn_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
	showErrorBox("not yet implemented!");
}


void
on_rulepropbtn_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
	showErrorBox("not yet implemented!");
}


void
on_ruleupbtn_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
	showErrorBox("not yet implemented!");
}


void
on_ruledownbtn_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
	showErrorBox("not yet implemented!");
}

