/*
   Liferea selection handling (derived from the GTK tutorial at
   http://www.gtk.org/tutorial/sec-supplyingtheselection.html)

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

#include <gtk/gtk.h>
#include <glib.h>
#include "htmlview.h"
#include "support.h"
#include "ui_selection.h"

static GtkWidget	*selection_widget = NULL;

/* ----------------------------------------------------------------------------	*/
/*  methods to supply a selection 						*/
/* ----------------------------------------------------------------------------	*/   

/* method to set selection */
void supplySelection(void) {

	gtk_selection_owner_set(selection_widget, GDK_SELECTION_PRIMARY, GDK_CURRENT_TIME);
}

/* Called when another application claims the selection */
static gboolean selection_clear(GtkWidget *widget, GdkEventSelection *event, gint *have_selection) {

	/* should we do anything ? */
	
	return TRUE;
}

/* Supplies the currently selected URL (if any) as the selection. */
static void selection_handle(GtkWidget *widget, GtkSelectionData *selection_data,
			     guint info, guint time_stamp, gpointer data) {
	gchar	*url;

	url = getSelectedURL();
	
	/* When we return a single string, it should not be null terminated.
	   That will be done for us */

	gtk_selection_data_set(selection_data, GDK_SELECTION_TYPE_STRING, 8, url, strlen(url));
}

/* sets up the selection widget and its handlers */
void setupSelection(void) { 

	if(NULL == selection_widget) {
		selection_widget = gtk_invisible_new();
		gtk_selection_add_target(selection_widget,
					 GDK_SELECTION_PRIMARY,
					 GDK_SELECTION_TYPE_STRING,
					 1);

		g_signal_connect(G_OBJECT(selection_widget), "selection_clear_event",
				 G_CALLBACK(selection_clear), NULL);
		g_signal_connect(G_OBJECT(selection_widget), "selection_get",
				 G_CALLBACK(selection_handle), NULL);
	}
}

/* ----------------------------------------------------------------------------	*/
/*  methods to get the selection 						*/
/* ----------------------------------------------------------------------------	*/   

/* method to request the selection */
void getSelection(GtkWidget *window) {
	static GdkAtom targets_atom = GDK_NONE;

	/* Get the atom corresponding to the string "TARGETS" */
	if(targets_atom == GDK_NONE)
  		targets_atom = gdk_atom_intern ("TARGETS", FALSE);

	/* And request the "TARGETS" target for the primary selection */
	gtk_selection_convert(window, GDK_SELECTION_PRIMARY, targets_atom, GDK_CURRENT_TIME);
}

/* Signal handler called when the selections owner returns the data */
static void selection_received(GtkWidget        *widget,
                               GtkSelectionData *selection_data, 
                               gpointer          data) {
	GdkAtom *atoms;
	GList *item_list;
	int i;

	/* **** IMPORTANT **** Check to see if retrieval succeeded  */
	if(selection_data->length < 0) {
		g_warning(_("Selection retrieval failed\n"));
		return;
	}
	/* Make sure we got the data in the expected form */
	if(selection_data->type != GDK_SELECTION_TYPE_ATOM) {
		g_warning(_("Selection \"TARGETS\" was not returned as atoms!\n"));
		return;
	}
  
	/* Print out the atoms we received */	
	atoms = (GdkAtom *)selection_data->data;

	item_list = NULL;
  for (i = 0; i < selection_data->length / sizeof(GdkAtom); i++)
    {
      char *name;
      name = gdk_atom_name (atoms[i]);
      if (name != NULL)
g_print ("%s\n",name);
      else
g_print ("(bad atom)\n");
    }
}
