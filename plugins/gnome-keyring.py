#
# GNOME Keyring Plugin
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
gi.require_version('GnomeKeyring', '1.0')
from gi.repository import GObject, Peas, PeasGtk, Gtk, Liferea, GnomeKeyring

class GnomeKeyringPlugin(GObject.Object, Liferea.AuthActivatable):
    __gtype_name__ = 'GnomeKeyringPlugin'

    object = GObject.property(type=GObject.Object)

    def do_activate(self):
	GnomeKeyring.unlock_sync("liferea", None)

    def do_deactivate(self):
        window = self.object

    def do_query(self, id):
        # Fetch secret by id
        attrs = GnomeKeyring.Attribute.list_new()
        GnomeKeyring.Attribute.list_append_string(attrs, 'id', id)
        result, value = GnomeKeyring.find_items_sync(GnomeKeyring.ItemType.GENERIC_SECRET, attrs)
        if result != GnomeKeyring.Result.OK:
          return

	#print 'password %s = %s' % (id, value[0].secret)
	#print 'password id = %s' % value[0].item_id

	username, password = value[0].secret.split('@@@')
  	Liferea.auth_info_from_store(id, username, password)

    def do_delete(self, id):
        keyring = GnomeKeyring.get_default_keyring_sync()[1]
        GnomeKeyring.item_delete_sync(keyring, id)

    def do_store(self, id, username, password):
	GnomeKeyring.create_sync("liferea", None)
        attrs = GnomeKeyring.Attribute.list_new()
        GnomeKeyring.Attribute.list_append_string(attrs, 'id', id)
        GnomeKeyring.Attribute.list_append_string(attrs, 'user', username)
	GnomeKeyring.item_create_sync("liferea", GnomeKeyring.ItemType.GENERIC_SECRET, repr(id), attrs, '@@@'.join([username, password]), True)

