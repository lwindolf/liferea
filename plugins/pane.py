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
import os
from pathlib import Path
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
    config_path = "liferea/plugins/pane"
    config_home = os.getenv('XDG_CONFIG_HOME',Path.joinpath(Path.home(), ".config"))
    return Path.joinpath(Path(config_home), config_path)


class PaneWorkaroundPlugin(GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'PaneWorkaroundPlugin'

    shell = GObject.property(type=Liferea.Shell)
    normal_pane = None
    wide_pane = None
    pos_normal = 199
    pos_wide = 561
    delay = 0.2

    def threaded_set_position(self):
        sleep(self.delay)
        self.normal_pane.set_position(self.pos_normal)
        self.wide_pane.set_position(self.pos_wide)

    def do_activate(self):
        self.normal_pane = self.shell.lookup('normalViewPane')
        self.wide_pane = self.shell.lookup('wideViewPane')
        self.read_position_from_file()
        thread = Thread(target = self.threaded_set_position, args = ())
        thread.start()

    def do_deactivate(self):
        self.normal_pane = self.shell.lookup('normalViewPane')
        self.wide_pane = self.shell.lookup('wideViewPane')
        cur_pos_normal = self.normal_pane.get_position()
        cur_pos_wide = self.wide_pane.get_position()
        if cur_pos_normal != self.pos_normal or cur_pos_wide != self.pos_wide:
            self.save_position_to_file(cur_pos_normal, cur_pos_wide)

    def read_position_from_file(self):
        path = get_path()
        file_path = path / FILE_CONFIG
        if file_path.exists():
            data = file_path.read_text()
            data = data.split(' ')
            self.pos_normal = int(data[0])
            self.pos_wide = int(data[1])

    def save_position_to_file(self, normal, wide):
        path = get_path()
        path.mkdir(0o700, True, True)
        file_path = path / FILE_CONFIG
        file_path.write_text(f'{normal} {wide}')
