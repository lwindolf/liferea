from gi.repository import GObject, Peas, PeasGtk, Gtk, Liferea, Gdk, Pango

def cell_func_bold_unread(column, cell, model, iter, data):
    unread_count = model.get(iter, 3)
    cell.set_property("weight", Pango.Weight.BOLD if (unread_count[0] > 0) else Pango.Weight.NORMAL)
    return

class NrBoldUnreadPlugin (GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'NrBoldUnreadPlugin'

    shell = GObject.property (type=Liferea.Shell)

    def do_activate (self):
        self.treeview = self.shell.lookup ("feedlist")
        self.ticol = self.treeview.get_column (0)
        area = self.ticol.get_property ("cell-area")
        self.layout = area.get_cells ()
        self.ticol.set_cell_data_func (self.layout[1], cell_func_bold_unread)
        self.treeview.queue_draw ()
        return

    def do_deactivate (self):
        self.ticol.set_cell_data_func (self.layout[1], None)
        self.treeview.queue_draw ()
        return
