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

# Initialize translations for tooltips
# Fallback to English if gettext module can't find the translations
# (That's possible if they are installed in a nontraditional dir)
import gettext
_ = lambda x: x
try:
    t = gettext.translation("liferea")
except FileNotFoundError:
    pass
else:
    _ = t.gettext

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
        button.set_action_name("app.prev-read-item")
        button.set_tooltip_text(_("Previous Item"))
        box.add(button)

        button = Gtk.Button()
        button.add(Gtk.Arrow(Gtk.ArrowType.RIGHT, Gtk.ShadowType.NONE))
        button.set_action_name("app.next-read-item")
        button.set_tooltip_text(_("Next Item"))
        box.add(button)

        button = Gtk.Button()
        icon = Gio.ThemedIcon(name="edit-redo-symbolic")
        image = Gtk.Image.new_from_gicon(icon, Gtk.IconSize.BUTTON)
        button.add(image)
        button.set_action_name("app.next-unread-item")
        button.set_tooltip_text(_("_Next Unread Item").replace("_", ""))
        box.add(button)

        button = Gtk.Button()
        icon = Gio.ThemedIcon(name="emblem-ok-symbolic")
        image = Gtk.Image.new_from_gicon(icon, Gtk.IconSize.BUTTON)
        button.add(image)
        button.set_action_name("app.mark-all-feeds-read")
        button.set_tooltip_text(_("_Mark Items Read").replace("_", ""))
        box.add(button)

        self.hb.pack_start(box)

        # Right side buttons
        button = Gtk.MenuButton()
        icon = Gio.ThemedIcon(name="open-menu-symbolic")
        image = Gtk.Image.new_from_gicon(icon, Gtk.IconSize.BUTTON)
        builder = self.shell.get_property("builder")
        button.set_menu_model(builder.get_object("menubar"))
        button.add(image)
        self.hb.pack_end(button)

        button = Gtk.Button()
        icon = Gio.ThemedIcon(name="edit-find-symbolic")
        image = Gtk.Image.new_from_gicon(icon, Gtk.IconSize.BUTTON)
        button.add(image)
        button.set_action_name("app.search-feeds")
        button.set_tooltip_text(_("Search All Feeds..."))
        self.hb.pack_end(button)

        self.shell.lookup("mainwindow").set_titlebar(self.hb)
        self.shell.lookup("mainwindow").set_show_menubar(False)
        self.shell.lookup("maintoolbar").set_visible(False)
        self.hb.show_all()

    def do_deactivate (self):
        self.shell.lookup("mainwindow").set_titlebar(None)
        self.shell.lookup("mainwindow").set_show_menubar(True)
        self.shell.lookup("maintoolbar").set_visible(True)
        self.hb = None
