#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <gtk/gtk.h>

extern GtkWidget	*mainwindow;

/* 2 or 3 pane mode flag from ui_mainwindow.c */
extern gboolean 	itemlist_mode;

/**
 * Switches between two and three pane mode.
 *
 * @param new_mode	TRUE if three pane mode is requested
 */
void switchPaneMode(gboolean new_mode);

/** According to the preferences this function enables/disables the toolbar */
void ui_mainwindow_update_toolbar();

/** According to the preferences this function enables/disables the menubar */
void ui_mainwindow_update_menubar();

/**
 * Sets the status bar text. Takes printf() like parameters 
 */
void ui_mainwindow_set_status_bar(const char *format, ...);

/* GUI callbacks */
void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_popup_toggle_condensed_view(gpointer cb_data, guint cb_action, GtkWidget *item);
void on_onlinebtn_clicked(GtkButton *button, gpointer user_data);

void
on_work_offline_activate               (GtkMenuItem     *menuitem,
								gpointer         user_data);

#endif
