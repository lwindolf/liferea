#
# libnotify Popup Notifications Plugin
#
# Copyright (C) 2013-2015 Lars Windolf <lars.windolf@gmx.de>
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
gi.require_version('Notify', '0.7')
from gi.repository import GObject, Peas, PeasGtk, Gtk, Liferea, Notify

class LibnotifyPlugin (GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'LibnotifyPlugin'

    object = GObject.property (type=GObject.Object)
    shell = GObject.property (type=Liferea.Shell)

    def do_activate (self):
	self._handler_id = self.shell.props.feed_list.connect ("node-updated", self.on_node_updated)

    def do_deactivate (self):
	self.shell.props.feed_list.disconnect (self._handler_id)

    def on_node_updated (self, widget, nodeTitle):
	Notify.init ('Liferea')
        notification = Notify.Notification.new (nodeTitle, "was updated", "dialog-information")
        notification.show () 
