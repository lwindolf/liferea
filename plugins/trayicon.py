#
# System Tray Icon Plugin
#
# Copyright (C) 2013 Lars Windolf <lars.lindner@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

import gi
gi.require_version('Peas', '1.0')
gi.require_version('PeasGtk', '1.0')
gi.require_version('Liferea', '3.0')
from gi.repository import GObject, Peas, PeasGtk, Gtk, Liferea, Gdk

class TrayiconPlugin (GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'TrayiconPlugin'

    object = GObject.property(type=GObject.Object)
    shell = GObject.property(type=Liferea.Shell)

    def do_activate(self):
        self.staticon = Gtk.StatusIcon ()
        # FIXME: Support a scalable image!
        self.staticon.set_from_pixbuf(Liferea.icon_create_from_file("unread.png"))
        self.staticon.connect("activate", self.trayicon_click)
        self.staticon.connect("popup_menu", self.trayicon_popup)
        self.staticon.set_visible(True)

        self.menu = Gtk.Menu()
        menuitem_toggle = Gtk.MenuItem("Show / Hide")
        menuitem_quit = Gtk.MenuItem("Quit")
        menuitem_toggle.connect("activate", self.trayicon_toggle)
        menuitem_quit.connect("activate", self.trayicon_quit)
        self.menu.append(menuitem_toggle)
        self.menu.append(menuitem_quit)
        self.menu.show_all()

        self.window = self.shell.get_window()
        self.minimize_to_tray_delete_handler = self.window.connect("delete_event",
                                                                   self.trayicon_minimize_on_close)
        self.minimize_to_tray_minimize_handler = self.window.connect("window-state-event",
                                                                     self.window_state_event_cb)

        # show the window if it is hidden when starting liferea
        self.window.deiconify()
        self.window.show()

    def window_state_event_cb(self, widget, event):
        "Hide window when minimize"
        if event.changed_mask & event.new_window_state & Gdk.WindowState.ICONIFIED:
            self.window.hide()

    def trayicon_click(self, widget, data = None):
        self.window.deiconify()
        self.window.show()

    def trayicon_minimize_on_close(self, widget, data = None):
        self.window.hide()
        return True

    def trayicon_toggle(self, widget, data = None):
        self.shell.toggle_visibility()

    def trayicon_quit(self, widget, data = None):
        Liferea.shutdown()

    def trayicon_popup(self, widget, button, time, data = None):
        self.menu.popup(None, None, self.staticon.position_menu, self.staticon, 3, time)

    def do_deactivate(self):
        self.staticon.set_visible(False)
        self.window.disconnect(self.minimize_to_tray_delete_handler)
        self.window.disconnect(self.minimize_to_tray_minimize_handler)

        # unhide the window when deactivating the plugin
        self.window.deiconify()
        self.window.show()

        del self.staticon
        del self.window
        del self.menu
