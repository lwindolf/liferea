#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <gtk/gtk.h>

extern GtkWidget	*mainwindow;

/* 2 or 3 pane mode flag from ui_mainwindow.c */
extern gboolean 	itemlist_mode;

/**
 * Create a new main window
 */
GtkWidget* ui_mainwindow_new();

void ui_mainwindow_finish(GtkWidget *window);

void ui_mainwindow_set_mode(gboolean threePane);
void ui_mainwindow_zoom_in();
void ui_mainwindow_zoom_out();

GtkWidget *ui_mainwindow_get_active_htmlview();

/** According to the preferences this function enables/disables the toolbar */
void ui_mainwindow_update_toolbar();

/** Set the sensitivity of items in the feed menu based on the type of item selected */
void ui_mainwindow_update_feed_menu(gint type);

/** According to the preferences this function enables/disables the menubar */
void ui_mainwindow_update_menubar();
/**
 * Sets the status bar text. Takes printf() like parameters 
 */
void ui_mainwindow_set_status_bar(const char *format, ...);

void ui_mainwindow_update_onlinebtn(void);

/* don't save off-screen positioning */

/**
 * Save the current mainwindow position to gconf, if the window is
 * shown and completely on the screen.
 */
void ui_mainwindow_save_position();

/**
 * Restore the window position from the values saved into gconf. Note
 * that this does not display/present/show the mainwindow.
 */
void ui_mainwindow_restore_position();
/* GUI callbacks */
void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_popup_toggle_condensed_view(gpointer cb_data, guint cb_action, GtkWidget *item);
void on_onlinebtn_clicked(GtkButton *button, gpointer user_data);
void on_work_offline_activate(GtkMenuItem *menuitem, gpointer user_data);

void
on_work_offline_activate               (GtkMenuItem     *menuitem,
								gpointer         user_data);

#endif
