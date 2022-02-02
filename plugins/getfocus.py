"""
GetFocus! Liferea plugin

Copyright (C) 2021-2022 Paweł Marciniak <sunwire+liferea@gmail.com

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
import gettext
from gi.repository import GObject, Gtk, Liferea, PeasGtk

# Initialize translations for tooltips
_ = lambda x: x
try:
    t = gettext.translation("liferea")
except FileNotFoundError:
    pass
else:
    _ = t.gettext

FILE_CONFIG = 'opacity.conf'


def get_path():
    config_path = "liferea/plugins/getfocus"
    config_home = os.getenv('XDG_CONFIG_HOME',Path.joinpath(Path.home(), ".config"))
    return Path.joinpath(Path(config_home), config_path)


class GetFocusPlugin(GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'GetFocusPlugin'

    shell = GObject.property(type=Liferea.Shell)
    feedlist = None
    opacity = 0.3
    enter_event = None
    leave_event = None

    def do_activate(self):
        self.feedlist = self.shell.lookup('feedlist')
        self.read_opacity_from_file()
        self.set_opacity_leave(self.feedlist, None)
        self.leave_event = self.feedlist.connect('leave-notify-event',
                                                 self.set_opacity_leave)
        self.enter_event = self.feedlist.connect('enter-notify-event',
                                                 self.set_opacity_enter)

    def do_deactivate(self):
        self.feedlist.disconnect(self.enter_event)
        self.feedlist.disconnect(self.leave_event)
        self.set_opacity_enter(self.feedlist, None)

    def set_opacity_enter(self, widget, event):
        self.opacity = widget.get_property('opacity')
        widget.set_property('opacity', 1)

    def set_opacity_leave(self, widget, event):
        widget.set_property('opacity', self.opacity)

    def read_opacity_from_file(self):
        path = get_path()
        file_path = path / FILE_CONFIG
        if file_path.exists():
            self.opacity = float(file_path.read_text())


class GetFocusConfigure(GObject.Object, PeasGtk.Configurable):
    __gtype_name__ = 'GetFocusConfigure'

    opacity = None
    feedlist = None
    opacity_scale = None

    def do_create_configure_widget(self):
        """ Setup configuration widget """
        margin = 6

        shell = Liferea.Shell
        self.feedlist = shell.lookup('feedlist')
        self.opacity = self.feedlist.get_property('opacity')

        grid = Gtk.Grid(column_spacing=10)
        label = Gtk.Label(_("Opacity:"))
        label.props.tooltip_text = _("Opacity")
        label.props.xalign = 0
        label.props.margin = margin
        label.props.expand = False
        grid.attach(label, 0, 0, 1, 1)

        adj = Gtk.Adjustment(self.opacity, 0, 1.0, 0.1, 0.2, 0)
        self.opacity_scale = Gtk.Scale(orientation=Gtk.Orientation.HORIZONTAL,
                                       adjustment=adj)
        self.opacity_scale.props.margin = 10
        self.opacity_scale.add_mark(0, Gtk.PositionType.BOTTOM, _('Min'))
        self.opacity_scale.add_mark(0.2, Gtk.PositionType.BOTTOM, None)
        self.opacity_scale.add_mark(0.4, Gtk.PositionType.BOTTOM, None)
        self.opacity_scale.add_mark(0.6, Gtk.PositionType.BOTTOM, None)
        self.opacity_scale.add_mark(0.8, Gtk.PositionType.BOTTOM, None)
        self.opacity_scale.add_mark(1.0, Gtk.PositionType.BOTTOM, _('Max'))
        self.opacity_scale.set_hexpand(True)
        self.opacity_scale.set_size_request(300, 10)  #width, height
        self.opacity_scale.connect("value-changed", self.scale_moved)
        grid.attach(self.opacity_scale, 1, 0, 1, 1)

        save_button = Gtk.Button(_('Save'))
        save_button.set_valign(Gtk.Align.CENTER)
        save_button.connect("clicked", self.save_opacity_to_file)
        grid.attach(save_button, 2, 0, 1, 1)
        return grid

    def scale_moved(self, event):
        self.opacity = self.opacity_scale.get_value()
        self.feedlist.set_property('opacity', self.opacity)

    def save_opacity_to_file(self, widget):
        path = get_path()
        path.mkdir(0o700, True, True)
        file_path = path / FILE_CONFIG
        file_path.write_text(str(self.opacity))
