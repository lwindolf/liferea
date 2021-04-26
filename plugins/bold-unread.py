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
