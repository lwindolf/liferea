#!/usr/bin/python3
# vim:fileencoding=utf-8:sw=4:et

"""
Open Website

Adds a "Tools" menu entry that opens a defined website in a new browser tab.
Change WEBSITE_URL and WEBSITE_TITLE below to point to the site you want.

Copyright (C) 2025 Lars Windolf <lars.windolf@gmx.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""

import gi

gi.require_version('Gtk', '4.0')

from gi.repository import GObject, Gio, Liferea

WEBSITE_URL = "https://lwindolf.github.io/rss-finder/"
WEBSITE_TITLE = "Discover Feeds"

ACTION_NAME = "OpenWebsite"


def remove_menuitem(action, menu):
    """Remove all entries of a Gio.Menu that trigger the given action"""
    for i in reversed(range(menu.get_n_items())):
        value = menu.get_item_attribute_value(i, "action", None)
        if value is not None and value.get_string() == action:
            menu.remove(i)


class OpenWebsitePlugin(GObject.Object, Liferea.Activatable, Liferea.ShellActivatable):
    __gtype_name__ = "OpenWebsitePlugin"

    shell = GObject.property(type=Liferea.Shell)

    def __init__(self):
        super().__init__()
        self._app = None
        self._menu = None

    def do_activate(self):
        action = Gio.SimpleAction.new(ACTION_NAME, None)
        action.connect("activate", self.on_open_website)

        self._app = Liferea.Shell.get_window().get_application()
        self._app.add_action(action)

        self._menu = self.shell.get_property("builder").get_object("tools_menu")
        self._menu.append(WEBSITE_TITLE, "app." + ACTION_NAME)

    def do_deactivate(self):
        if self._app:
            self._app.remove_action(ACTION_NAME)
            self._app = None
        if self._menu:
            remove_menuitem("app." + ACTION_NAME, self._menu)
            self._menu = None

    def on_open_website(self, action, param):
        Liferea.BrowserTabs.add_new(WEBSITE_URL, WEBSITE_TITLE, True)
