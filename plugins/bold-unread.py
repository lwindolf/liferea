"""
bold unread plugin

Copyright (C) 2017 Yanko Kaneti <yaneti@declera.com>

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

from gi.repository import GObject, Liferea, Pango

def cell_func_bold_unread(column, cell, model, iterator, data):
    unread_count = model.get(iterator, 3)
    cell.set_property("weight", Pango.Weight.BOLD if (unread_count[0] > 0) else Pango.Weight.NORMAL)

class NrBoldUnreadPlugin(GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'NrBoldUnreadPlugin'
    treeview = None
    ticol = None
    layout = None
    shell = GObject.property(type=Liferea.Shell)

    def do_activate(self):
        self.treeview = self.shell.lookup("feedlist")
        self.ticol = self.treeview.get_column(0)
        area = self.ticol.get_property("cell-area")
        self.layout = area.get_cells()
        self.ticol.set_cell_data_func(self.layout[1], cell_func_bold_unread)
        self.treeview.queue_draw()

    def do_deactivate(self):
        self.ticol.set_cell_data_func(self.layout[1], None)
        self.layout[1].set_property("weight", Pango.Weight.NORMAL)
        self.treeview.queue_draw()
