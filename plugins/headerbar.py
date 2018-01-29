#!/usr/bin/python3
# vim:fileencoding=utf-8:sw=4:et
#
# Liferea Header Bar Plugin
#
# Copyright (C) 2018 Lars Windolf <lars.windolf@gmx.de>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.
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

gi.require_version('Gtk', '3.0')

from gi.repository import GObject, Gio, Gtk, Gdk, PeasGtk, Liferea

class HeaderBarPlugin (GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = "HeaderBarPlugin"

    object = GObject.property (type=GObject.Object)
    shell = GObject.property (type=Liferea.Shell)

    def __init__(self):
        GObject.Object.__init__(self)

    def do_activate (self):
        self.hb = Gtk.HeaderBar()
        self.hb.props.show_close_button = True
        self.hb.set_title("Liferea")

        # Left side buttons
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        Gtk.StyleContext.add_class(box.get_style_context(), "linked")

        button = Gtk.Button()
        button.add(Gtk.Arrow(Gtk.ArrowType.LEFT, Gtk.ShadowType.NONE))
        button.connect("clicked", self._on_back)
        box.add(button)

        button = Gtk.Button()
        button.add(Gtk.Arrow(Gtk.ArrowType.RIGHT, Gtk.ShadowType.NONE))
        # FIXME: signal
        box.add(button)

        self.hb.pack_start(box)

        # Right side buttons
        button = Gtk.Button()
        icon = Gio.ThemedIcon(name="open-menu-symbolic")
        image = Gtk.Image.new_from_gicon(icon, Gtk.IconSize.BUTTON)
        # FIXME: signal
        button.add(image)
        self.hb.pack_end(button)

        button = Gtk.Button()
        icon = Gio.ThemedIcon(name="edit-find-symbolic")
        image = Gtk.Image.new_from_gicon(icon, Gtk.IconSize.BUTTON)
        # FIXME: signal
        button.add(image)
        self.hb.pack_end(button)

        self.shell.lookup("mainwindow").set_titlebar(self.hb)

        ui_manager = self.shell.get_property("ui-manager")
        ui_manager.get_widget("/maintoolbar/").set_visible(False)
        ui_manager.get_widget("/MainwindowMenubar/").set_visible(False)
        self.hb.show_all()

    def do_deactivate (self):
        ui_manager = self.shell.get_property("ui-manager")
        ui_manager.get_widget("/maintoolbar/").set_visible(True)
        ui_manager.get_widget("/MainwindowMenubar/").set_visible(True)
        self.hb = None

    def _on_back(self, button):
        print("FIXME: do something")

