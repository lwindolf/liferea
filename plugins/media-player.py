from gi.repository import GObject
from gi.repository import Peas
from gi.repository import PeasGtk
from gi.repository import GLib
from gi.repository import Gtk
from gi.repository import Liferea
from gi.repository import Gst

class MediaPlayerPlugin(GObject.Object, Liferea.MediaPlayerActivatable):
    __gtype_name__ = 'MediaPlayerPlugin'

    object = GObject.property(type=GObject.Object)

    def __init__(self):
        Gst.init_check(None)
	self.player = Gst.ElementFactory.make("playbin2", "player")
	fakesink = Gst.ElementFactory.make("fakesink", "fakesink")
	self.player.set_property("video-sink", fakesink)
	bus = self.player.get_bus()
	bus.add_signal_watch_full()
	bus.connect("message", self.on_message)

    def on_message(self, bus, message):
	t = message.type
	if t == Gst.Message.EOS:
		self.player.set_state(Gst.State.NULL)
		self.playing = False
	elif t == Gst.Message.ERROR:
		self.player.set_state(Gst.State.NULL)
		err, debug = message.parse_error()
		print "Error: %s" % err, debug
		self.playing = False

	self.updateButtons()

    def play(self):
	uri = Liferea.enclosure_get_url(self.enclosures[self.pos])
	self.player.set_property("uri", uri)
	self.player.set_state(Gst.State.PLAYING)
	Liferea.ItemView.select_enclosure(self.pos)

    def stop(self):
	self.player.set_state(Gst.State.NULL)
	
    def playToggled(self, w):
	if(self.playing == False):
		self.play()
	else:
		self.stop()

	self.playing=not(self.playing)
	self.updateButtons()

    def next(self, w):
	self.stop()
        self.pos+=1
	self.play()
	self.updateButtons()

    def prev(self, w):
	self.stop()
        self.pos-=1
	self.play()
	self.updateButtons()       

    def updateButtons(self):
        Gtk.Widget.set_sensitive(self.prevButton, (self.pos != 0))
	Gtk.Widget.set_sensitive(self.nextButton, (len(self.enclosures) - 1 > self.pos))

        if(self.playing == False):
           self.playButtonImage.set_from_stock("gtk-media-play", Gtk.IconSize.BUTTON)
        else:
           self.playButtonImage.set_from_stock("gtk-media-pause", Gtk.IconSize.BUTTON)

    def do_load(self, parentWidget, enclosures):
        if parentWidget == None:
           print "ERROR: Could not find media player insertion widget!"

        # Test wether Media Player widget already exists
        childList = Gtk.Container.get_children(parentWidget)

	if len(childList) == 1:
	   # We need to add a media player...
	   vbox = Gtk.Box(Gtk.Orientation.HORIZONTAL, 0)
	   Gtk.Box.pack_start(parentWidget, vbox, True, True, 0);

	   label = Gtk.Label(label='Media Player')

	   image = Gtk.Image()
           image.set_from_stock("gtk-media-previous", Gtk.IconSize.BUTTON)
           self.prevButton = Gtk.Button.new()
           self.prevButton.add(image)
           self.prevButton.connect("clicked", self.prev)
	   Gtk.Box.pack_start(vbox, self.prevButton, False, False, 0)

           self.playButtonImage = Gtk.Image()
           self.playButtonImage.set_from_stock("gtk-media-play", Gtk.IconSize.BUTTON)
           self.playButton = Gtk.Button.new()
           self.playButton.add(self.playButtonImage)
           self.playButton.connect("clicked", self.playToggled)
	   Gtk.Box.pack_start(vbox, self.playButton, False, False, 0)

	   image = Gtk.Image()
           image.set_from_stock("gtk-media-next", Gtk.IconSize.BUTTON)
           self.nextButton = Gtk.Button.new()
           self.nextButton.add(image)
           self.nextButton.connect("clicked", self.next)
	   Gtk.Box.pack_start(vbox, self.nextButton, False, False, 0)

	   Gtk.Box.pack_start(vbox, label, True, True, 0)

	   Gtk.Widget.show_all(vbox)

	# And fill it with the enclosures
	self.enclosures = enclosures
        for enclosure in self.enclosures:
           print enclosure

	self.pos = 0
	self.playing = False
        self.player.set_state(Gst.State.NULL)	# FIXME: Make this configurable?
	self.updateButtons()

    def do_activate(self):
	print "=== MediaPlayer activate"

    def do_deactivate(self):
        window = self.object

