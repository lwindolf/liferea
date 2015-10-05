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

    object = GObject.property (type=GObject.Object)
    shell = GObject.property (type=Liferea.Shell)

    def do_activate (self):
        self.staticon = Gtk.StatusIcon ()
        # FIXME: Support a scalable image!
        self.staticon.set_from_pixbuf (Liferea.icon_create_from_file ("unread.png"))
        self.staticon.connect ("activate", self.trayicon_activate)
        self.staticon.connect ("popup_menu", self.trayicon_popup)
        self.staticon.set_visible (True)
        self.maximizing = False
        window = self.shell.get_window ()
        #self.minimize_to_tray_handler_id = window.connect('window-state-event', self.window_state_event_cb)

    def window_state_event_cb(self, window, event):
        window = self.shell.get_window ()
        state = Gtk.Widget.get_visible (window)
        if state:
            if event.new_window_state & Gdk.WindowState.ICONIFIED:
                if event.new_window_state & Gdk.WindowState.FOCUSED:
                    if event.new_window_state & Gdk.WindowState.WITHDRAWN:
                        self.maximizing = True
                    elif self.maximizing:
                        self.maximizing = False
                        window.present()
                    else:
                        Gtk.Widget.hide(window)

    def trayicon_activate (self, widget, data = None):
        window = self.shell.get_window ()
        state = Gtk.Widget.get_visible (window)
	self.shell.toggle_visibility()
    	return False

    def trayicon_quit (self, widget, data = None):
        Liferea.shutdown ()

    def trayicon_popup (self, widget, button, time, data = None):
        self.menu = Gtk.Menu ()

        menuitem_toggle = Gtk.MenuItem ("Show / Hide")
        menuitem_quit = Gtk.MenuItem ("Quit")

        menuitem_toggle.connect ("activate", self.trayicon_activate)
        menuitem_quit.connect ("activate", self.trayicon_quit)

        self.menu.append (menuitem_toggle)
        self.menu.append (menuitem_quit)

        self.menu.show_all ()
        self.menu.popup(None, None, lambda w,x: self.staticon.position_menu(self.menu, self.staticon), self.staticon, 3, time)

    def do_deactivate (self):
        self.staticon.set_visible (False)
        window = self.shell.get_window ()
        #window.disconnect(self.minimize_to_tray_handler_id)
        del self.staticon
