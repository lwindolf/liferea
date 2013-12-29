from gi.repository import GObject, Peas, PeasGtk, Gtk, Liferea

class TrayiconPlugin (GObject.Object, Liferea.ShellActivatable):
    __gtype_name__ = 'TrayiconPlugin'

    object = GObject.property (type=GObject.Object)
    shell = GObject.property (type=Liferea.Shell)

    def do_activate (self):
        self.staticon = Gtk.StatusIcon ()
	#self.icon.set_from_file(self.myicon)
        self.staticon.set_from_stock (Gtk.STOCK_ABOUT)
        self.staticon.connect ("activate", self.trayicon_activate)
        self.staticon.connect ("popup_menu", self.trayicon_popup)
        self.staticon.set_visible (True)

    def trayicon_activate (self, widget, data = None):
	window = self.shell.get_window ()
	state = Gtk.Widget.get_visible (window)
	if True == state:
		Gtk.Widget.hide (window)
	else:
		Gtk.Window.present (window)

    def trayicon_popup (self, widget, button, time, data = None):
        print "popup menu triggered"

    def do_deactivate (self):
        self.staticon.set_visible (False)
        del self.staticon
