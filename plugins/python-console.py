# Copyright (C) 2006 - Steve Frécinaux
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# The Rhythmbox authors hereby grant permission for non-GPL compatible
# GStreamer plugins to be used and distributed together with GStreamer
# and Rhythmbox. This permission is above and beyond the permissions granted
# by the GPL license by which Rhythmbox is covered. If you modify this code
# you may extend this exception to your version of the code, but you are not
# obligated to do so. If you do not wish to do so, delete this exception
# statement from your version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

# Parts from "Interactive Python-GTK Console" (stolen from epiphany's console.py)
#     Copyright (C), 1998 James Henstridge <james@daa.com.au>
#     Copyright (C), 2005 Adam Hooper <adamh@densi.com>
# Bits from gedit Python Console Plugin
#     Copyrignt (C), 2005 Raphaël Slinckx# Copyright (C) 2006 - Steve Frécinaux
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# The Rhythmbox authors hereby grant permission for non-GPL compatible
# GStreamer plugins to be used and distributed together with GStreamer
# and Rhythmbox. This permission is above and beyond the permissions granted
# by the GPL license by which Rhythmbox is covered. If you modify this code
# you may extend this exception to your version of the code, but you are not
# obligated to do so. If you do not wish to do so, delete this exception
# statement from your version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

# Parts from "Interactive Python-GTK Console" (stolen from epiphany's console.py)
#     Copyright (C), 1998 James Henstridge <james@daa.com.au>
#     Copyright (C), 2005 Adam Hooper <adamh@densi.com>
# Bits from gedit Python Console Plugin
#     Copyrignt (C), 2005 Raphaël Slinckx

# TODO
#  Handle primary selection, (middle mouse button paste)
#  on first keypress, move input to right location
#  more keyboard shortcuts:
#      http://www.keyxl.com/aaaf192/83/Linux-Bash-Shell-keyboard-shortcuts.htm
#      ctrl+r search history, tab for autocomplete etc..
# Replace console with a sourceview?
#      https://cfoch.github.io/tech/2017/08/24/tutorial-writing-a-plugin-in-pitivi-2017.html
# Steal ideas from Rb console?
#      https://github.com/GNOME/rhythmbox/blob/master/plugins/pythonconsole/pythonconsole.py

import gi
from gettext import gettext as _

from gi.repository import GObject, Gtk, Gio
from gi.repository import Liferea

from pc import PythonConsole

def remove_menuitem(action, menus, level=0):
    """
    Given an action name such as app.function, remove it from a Gio.Menu
    """
    for i in range(menus.get_n_items()):
        link = menus.iterate_item_links(i)
        if link.next():
            remove_menuitem(action, link.get_value(), level+1)
        else:            
            attr = menus.iterate_item_attributes(i)
            while attr.next():
                if str(attr.get_name()) == "action":
                    value = str(attr.get_value()).strip("'")
                    if value == action:
                        menus.remove(i)

class PythonConsolePlugin(GObject.Object, Liferea.Activatable, Liferea.ShellActivatable):
    __gtype_name__ = 'PythonConsolePlugin'

    shell = GObject.property(type=Liferea.Shell)
    
    def __init__(self):
        super().__init__()
        self.console_window = None

    def do_activate(self):
        action = Gio.SimpleAction.new('PythonConsole', None)
        action.connect("activate", self.show_console, self.shell)

        self._app = self.shell.get_window().get_application ()
        self._app.add_action(action)

        self.toolsmenu = self.shell.get_property("builder").get_object("tools_menu")
        self.toolsmenu.append('Python Console', 'app.PythonConsole')

    def do_deactivate(self):
        self._app.remove_action('PythonConsole')
        remove_menuitem('app.PythonConsole', self.toolsmenu)

        if self.console_window is not None:
            self.console_window.destroy()

    def show_console(self, action, variant, shell):
        if not self.console_window:
            ns = {'__builtins__' : __builtins__, 
                  'Liferea' : Liferea,
                  'shell' : shell}
            console = PythonConsole(namespace = ns, 
                                    destroy_cb = self.destroy_console)
            console.set_size_request(600, 400)
            console.eval('print("' + \
                         _("You can access the main window " \
                         "through the \'shell\' variable :") +
                         '\\n%s" % shell)', False)
            msg = "Run code using exec(open(\"/path/to/code.py\").read())"
            console.eval('print(\'%s\')' % msg, False)

            self.console_window = Gtk.Window()
            self.console_window.set_title('Lifera Python Console')
            self.console_window.set_child(console)
            self.console_window.connect('close-request', self.hide_console)

        self.console_window.present()
    
    def destroy_console(self, *args):
        self.console_window.destroy()
        self.console_window = None

    def hide_console(self, *args):
        self.console_window.hide()
        return True
