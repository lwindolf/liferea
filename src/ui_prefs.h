/**
 * @file ui_prefs.h program preferences
 *
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <gtk/gtk.h>
#include <gdk/gdk.h>

void 
on_prefbtn_clicked                     (GtkButton       *button,
                                        gpointer user_data);

void
on_updatealloptionbtn_clicked          (GtkButton       *button,
                                        gpointer         user_data);

void
on_trayiconoptionbtn_clicked           (GtkButton       *button,
                                        gpointer         user_data);
										
void
on_popupwindowsoptionbtn_clicked       (GtkButton       *button,
                                        gpointer         user_data);

void
on_browserselection_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_browsercmd_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_timeformatselection_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_timeformatentry_changed             (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_itemCountBtn_value_changed          (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_menuselection_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_helpoptionbtn_clicked               (GtkButton       *button,
                                        gpointer         user_data);
					
void
on_enableproxybtn_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_proxyhostentry_changed              (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_proxyportentry_changed              (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_openlinksinsidebtn_clicked          (GtkButton       *button,
                                        gpointer         user_data);
