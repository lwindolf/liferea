#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <gtk/gtk.h>

extern GtkWidget		*mainwindow;

void switchPaneMode(gboolean new_mode);
void ui_mainwindow_update_toolbar();
void ui_mainwindow_update_menubar();
void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_popup_toggle_condensed_view(gpointer cb_data, guint cb_action, GtkWidget *item);
void ui_mainwindow_set_status_bar(const char *format, ...);

#endif
