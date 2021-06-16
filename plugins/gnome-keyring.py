"""
Libsecret Plugin

Copyright (C) 2013 Lars Windolf <lars.lindner@gmail.com>
Copyright (C) 2018 Bastian Germann <bastiangermann@fishpost.de>

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
gi.require_version('Peas', '1.0')
gi.require_version('PeasGtk', '1.0')
gi.require_version('Liferea', '3.0')
gi.require_version('Secret', '1')
from gi.repository import GObject, Peas, PeasGtk, Gtk, Liferea, Secret

LABEL = 'liferea'
SCHEMA = Secret.Schema.new(
    "net.sf.liferea.Secret",
    Secret.SchemaFlags.NONE,
    {'id': Secret.SchemaAttributeType.STRING}
)

class LibsecretPlugin(GObject.Object, Liferea.AuthActivatable):
    __gtype_name__ = 'LibsecretPlugin'

    object = GObject.property(type=GObject.Object)

    def get_collection(self):
        service = Secret.Service.get_sync(Secret.ServiceFlags.LOAD_COLLECTIONS, None)
        colls = service.get_collections()
        for c in colls:
            if c.get_label() == LABEL:
                return c
        return None

    def do_deactivate(self):
        window = self.object

    def do_query(self, id):
        coll = self.get_collection()
        if coll:
            items = coll.search_sync(None, {'id': id}, Secret.SearchFlags.UNLOCK, None)
            if items and items[0].load_secret_sync(None):
                username, password = items[0].get_secret().get_text().split('@@@')
                Liferea.auth_info_from_store(id, username, password)

    def do_delete(self, id):
        Secret.password_clear_sync(SCHEMA, {'id': id}, None)

    def do_store(self, id, username, password):
        coll = self.get_collection()
        if not coll:
            coll = Secret.Collection.create_sync(None, LABEL, None, Secret.CollectionCreateFlags(0), None)
        login = '@@@'.join([username, password])
        secret = Secret.Value.new(login, len(login), 'text/plain')
        Secret.Item.create_sync(coll, SCHEMA, {'id': id}, repr(id), secret, Secret.ItemCreateFlags.REPLACE, None)
