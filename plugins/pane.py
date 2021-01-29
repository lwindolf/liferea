"""
Copyright (C) 2021 Paweł Marciniak <sunwire+liferea@gmail.com

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
import pathlib
import gettext
from threading import Thread
from time import sleep
from gi.repository import GObject, Gtk, Liferea, PeasGtk

# Initialize translations for tooltips
_ = lambda x: x
try:
    t = gettext.translation("liferea")
except FileNotFoundError:
    pass
else:
    _ = t.gettext

FILE_CONFIG = 'pane.conf'


def get_path():
    return pathlib.Path.joinpath(pathlib.Path.home(),
                                 ".config/liferea/plugins/pane")


class PaneWorkaroundPlugin(GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'PaneWorkaroundPlugin'

    shell = GObject.property(type=Liferea.Shell)
    normal_pane = None
    position = 199
    delay = 0.2

    def threaded_set_position(self):
        sleep(self.delay)
        self.normal_pane.set_position(self.position)

    def do_activate(self):
        self.normal_pane = self.shell.lookup('normalViewPane')
        self.read_position_from_file()
        thread = Thread(target = self.threaded_set_position, args = ())
        thread.start()

    def do_deactivate(self):
        self.normal_pane = self.shell.lookup('normalViewPane')
        current_position = self.normal_pane.get_position()
        if current_position != self.position:
            self.save_position_to_file(current_position)

    def read_position_from_file(self):
        path = get_path()
        file_path = path / FILE_CONFIG
        if file_path.exists():
            self.position = int(file_path.read_text())

    def save_position_to_file(self, position):
        path = get_path()
        path.mkdir(0o700, True, True)
        file_path = path / FILE_CONFIG
        file_path.write_text(str(position))
