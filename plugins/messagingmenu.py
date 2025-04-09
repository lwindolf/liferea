"""
MessagingMenu Plugin

Copyright (C) 2023 Tasos Sahanidis <code@tasossah.com>

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

import gettext
import html
import gi
gi.require_version("Gtk", "3.0")
gi.require_version("GLib", "2.0")
gi.require_version("MessagingMenu", "1.0")

from gi.repository import GObject, Gtk, Gio, MessagingMenu, Liferea

_ = lambda x: x
try:
    t = gettext.translation("liferea")
except FileNotFoundError:
    pass
else:
    _ = t.gettext

class MessagingMenuPlugin(GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = "MessagingMenuPlugin"

    object = GObject.property(type=GObject.Object)
    shell = GObject.property(type=Liferea.Shell)
    mmapp = None
    window = None
    treeview = None
    model = None
    delete_signal_id = None
    new_only = False
    sources = []

    def _source_activated(self, app, source_id):
        # Clear all sources and re-add them because it's the only way to preserve the order...
        for path in self.sources:
            app.remove_source(path)

        # This needs to be done because _on_node_changed() re-adds the entries
        sources_copy = list(self.sources)
        self.sources.clear()
        for path in sources_copy:
            path_iter = self.model.get_iter_from_string(path)
            self._on_node_changed(self.model, path, path_iter, app)

        # Show (if hidden) and focus window
        self.window.present()

        # Set selection to correct item based on source_id
        selection = self.treeview.get_selection()
        source_id_path = Gtk.TreePath.new_from_string(source_id)
        selection.select_path(source_id_path)

    def _on_node_changed(self, model, path, iter, mmapp):
        # path is TreePath when called from "row-changed"
        path = str(path)

        # If the node has children, ignore it as we need to flatten the tree
        if model.iter_has_child(iter):
            return

        # Slightly hacky, but certain GThemedIcons are used only for non-feed entries, so ignore them
        node_icon = model.get_value(iter, 1)
        if isinstance(node_icon, Gio.ThemedIcon):
            if any("folder-saved-search" in n for n in node_icon.get_names()):
                return
            # FIXME: The icon is broken otherwise (default rss icon)
            node_icon = None

        node_unread = model.get_value(iter, 3)

        # Remove sources that have 0 unread nodes
        source_exists = mmapp.has_source(path)
        if node_unread == 0:
            #print("zero")
            if source_exists:
                mmapp.remove_source(path)
                self.sources.remove(path)
        elif source_exists:
            #print("exists")
            # Update source count if already exists
            mmapp.set_source_count(path, node_unread)
        else:
            #print("new")
            # Add new source
            # WARNING: Python 3.4+
            node_text = html.unescape(model.get_value(iter, 0))
            print(node_text)
            mmapp.append_source_with_count(path, node_icon, node_text, node_unread)
            self.sources.append(path)

    def minimize_on_close(self, widget, event):
        self.window.hide()
        return True

    def do_activate(self):
        self.mmapp = MessagingMenu.App(desktop_id = "net.sourceforge.liferea.desktop")
        self.mmapp.register()
        self.mmapp.connect("activate-source", self._source_activated)

        # Get GtkTreeStore and connect signals
        self.treeview = self.shell.lookup("feedlist")
        self.model = self.treeview.get_model()
        self.model.connect("row-changed", self._on_node_changed, self.mmapp)

        # Update MessagingMenu on first start
        if not self.new_only:
            self.model.foreach(self._on_node_changed, self.mmapp)

        # Hide on close
        self.window = self.shell.get_window()
        self.delete_signal_id = GObject.signal_lookup("delete_event", Gtk.Window)
        GObject.signal_handlers_block_matched(self.window, GObject.SignalMatchType.ID | GObject.SignalMatchType.DATA, self.delete_signal_id, 0, None, None, None)
        self.window.connect("delete_event", self.minimize_on_close)

    def do_deactivate(self):
        self.mmapp.unregister()
        self.mmapp = None

        # Undo hide on close
        self.window.disconnect_by_func(self.minimize_on_close)
        GObject.signal_handlers_unblock_matched (self.window, GObject.SignalMatchType.ID | GObject.SignalMatchType.DATA, self.delete_signal_id, 0, None, None, None)
