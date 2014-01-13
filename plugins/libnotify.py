#
# libnotify Popup Notifications Plugin
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

from gi.repository import GObject, Peas, PeasGtk, Gtk, Liferea, Notify

class LibnotifyPlugin (GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'LibnotifyPlugin'

    object = GObject.property (type=GObject.Object)
    shell = GObject.property (type=Liferea.Shell)

    def do_activate (self):
	self.shell.props.feed_list.connect("new-items", self.on_new_items)

    def on_new_items (self, widget, title = None):
	Notify.init('Liferea')
	# FIXME: icon
        notification = Notify.Notification.new(
          title,
	  # FIXME: add list of first 5 items...
          'Items',
          'dialog-information'
        )
        notification.show()
