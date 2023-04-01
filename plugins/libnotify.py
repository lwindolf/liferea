"""
libnotify Popup Notifications Plugin

Copyright (C) 2013-2015 Lars Windolf <lars.windolf@gmx.de>
Copyright (C) 2020 Tasos Sahanidis <tasos@tasossah.com>

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
import gi
gi.require_version('Notify', '0.7')
from gi.repository import GObject, Liferea, Notify

_ = lambda x: x
try:
    t = gettext.translation("liferea")
except FileNotFoundError:
    pass
else:
    _ = t.gettext


class LibnotifyPlugin(GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'LibnotifyPlugin'

    object = GObject.property (type=GObject.Object)
    shell = GObject.property (type=Liferea.Shell)
    notification = None
    notification_title = _("Feed Updates")
    notification_body = ""
    notification_icon = "net.sourceforge.liferea"
    _handler_id = None

    def do_activate(self):
        Notify.init('Liferea')
        self._handler_id = self.shell.props.feed_list.connect("node-updated", self.on_node_updated)
        self.notification = Notify.Notification.new(self.notification_title, self.notification_body, self.notification_icon)
        self.notification.connect("closed", self.on_closed)

    def do_deactivate(self):
        Notify.uninit()
        self.shell.props.feed_list.disconnect(self._handler_id)

    def on_node_updated(self, widget, node_title):
        # Update the existing notification if it hasn't been closed yet
        if self.notification_body:
            self.notification_body += "\n" + node_title
        else:
            self.notification_body = node_title
        self.notification.update(self.notification_title, self.notification_body, self.notification_icon)
        self.notification.show()

    def on_closed(self, data):
        self.notification_body = ""
