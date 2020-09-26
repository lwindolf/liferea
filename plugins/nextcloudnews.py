#
# NextCloud News Plugin
#
# Copyright (C) 2020 Lars Windolf <lars.lindner@gmail.com>
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

import os
import gi

gi.require_version('Peas', '1.0')
gi.require_version('PeasGtk', '1.0')
gi.require_version('Liferea', '3.0')

from gi.repository import GObject, Peas, PeasGtk, Gtk, Liferea

UI_FILE_PATH = os.path.join(os.path.dirname(__file__), "nextcloudnews.ui")

class NextCloudNewsPlugin(GObject.Object, Liferea.NodeSourceActivatable):
    __gtype_name__ = 'NextCloudNewsPlugin'

    object = GObject.property(type=GObject.Object)

    def do_activate(self):
        print("NextCloud on")

    def do_deactivate(self):
        print("NextCloud off")

    def do_get_id(self):
        print("id=nextcloud")
        return "nextcloud"

    def do_get_name(self):
        return "NextCloud News"

    def do_new(self):
        """Present server/auth configure dialog"""
        print("new")

        GMARGIN = 6
        builder = self.builder = Gtk.Builder()
        builder.add_from_file(UI_FILE_PATH)
        handlers = {
            "okbutton1_activated_cb": self.on_subscribe,
            "cancelbutton1_activated_cb": self.on_cancel
        }
        builder.connect_signals(handlers)

    def on_subscribe(self, widget, *data):
        print("subscribe")

    def on_cancel(self, widget, *data):
        print("cancel subscribe")

    # FIXME
    def do_get_capabilities(self):
        return (Liferea.nodeSourceCapability.DYNAMIC_CREATION |
                Liferea.nodeSourceCapability.CAN_LOGIN |
                Liferea.nodeSourceCapability.WRITABLE_FEEDLIST |
                Liferea.nodeSourceCapability.ADD_FEED)
