#!/usr/bin/python3
# vim:fileencoding=utf-8:sw=4:et

"""
Add social bookmark site

Copyright (C) 2022-2025 Lars Windolf <lars.windolf@gmx.de>

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

import os
from pathlib import Path
import gi

gi.require_version('Gtk', '4.0')

from gi.repository import GObject, Gtk, Liferea

UI_FILE_PATH = os.path.join(os.path.dirname(__file__), "add-bookmark-site.ui")

""" get some good config file name """
config_path = "liferea/plugins/add-bookmark-site"
config_dir = os.getenv('XDG_CONFIG_HOME', Path.joinpath(Path.home(), ".config"))
config_dir = Path.joinpath(Path(config_dir), config_path)
config_fname = os.path.join(config_dir, "add-bookmark-site.ini")


def save_config(name, url):
    if not os.path.exists(config_dir):
        os.makedirs(config_dir)

    with open(config_fname, 'w', encoding='utf-8') as f:
        f.write(f"{name}|||{url}\n")

    register(name, url)

def load_config():
    try:
        with open(config_fname) as f:
            fields = f.readline().strip().split("|||")
            name = fields[0]
            url = fields[1]
    except Exception as e:
        print(e)
        name = ''
        url = ''

    return (name, url)

def register(name, url):
    if not name or not url:
        return

    # Unregister first to avoid duplicate entries
    Liferea.social_unregister_bookmark_site(name)
    Liferea.social_register_bookmark_site(name, url)
    Liferea.social_set_bookmark_site(name)
    return

class AddBookmarkSitePlugin (GObject.Object, Liferea.Activatable, 
        Liferea.ShellActivatable):
    __gtype_name__ = "AddBookmarkSitePlugin"

    shell = GObject.property (type=Liferea.Shell)
    _nameEntry = None
    _urlEntry = None

    def __init__(self):
        GObject.Object.__init__(self)

    def do_activate (self):
        name, url = load_config()
        register(name, url)

    def do_deactivate (self):
        """On shutdown we just leave the registered bookmark service
           it will be automatically gone after next restart"""
        return

    def do_create_configure_widget(self):
        builder = Gtk.Builder()
        builder.add_from_file(UI_FILE_PATH)
        builder.connect_signals({})

        self._urlEntry = builder.get_object("urlEntry")
        self._nameEntry = builder.get_object("nameEntry")
        
        name, url = load_config()
        self._urlEntry.set_text(url)
        self._nameEntry.set_text(name)

        applyBtn = builder.get_object("applyBtn")
        applyBtn.connect('clicked', self.on_save_cb)
        
        self.dialog = builder.get_object("configDialog")
        self.dialog.show_all()
        self.dialog.run()

    def on_save_cb(self, user_data):
        save_config(self._nameEntry.get_text(), self._urlEntry.get_text())
        self.dialog.destroy()
        self.dialog = None
        self._urlEntry = None
        self._nameEntry = None
        return
