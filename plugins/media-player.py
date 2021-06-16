"""
GStreamer based embedded Media Player

Copyright (C) 2013 Lars Windolf <lars.lindner@gmail.com>
Copyright (C) 2013 Simon Kagedal Reimer <skagedal@gmail.com>

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

import gi
gi.require_version('Gst', '1.0')
gi.require_version('Peas', '1.0')
gi.require_version('PeasGtk', '1.0')
gi.require_version('Liferea', '3.0')
from gi.repository import GObject, Peas, PeasGtk, GLib, Gtk, Liferea, Gst

GMARGIN = 6
def enum(*sequential, **named):
    """Create an ENUM data type"""
    enums = dict(zip(sequential, range(len(sequential))), **named)
    return type('Enum', (), enums)

PlayState = enum("PLAY", "PAUSE", "STOP")

class MediaPlayerPlugin(GObject.Object, Liferea.MediaPlayerActivatable):
    __gtype_name__ = 'MediaPlayerPlugin'

    object = GObject.property(type=GObject.Object)

    def __init__(self):
        Gst.init_check(None)
        self.IS_GST010 = Gst.version() < (0, 11)

        playbin = "playbin2" if self.IS_GST010 else "playbin"
        self.playing = PlayState.STOP
        self.player = Gst.ElementFactory.make(playbin, "player")
        if not self.IS_GST010:
            fakesink = Gst.ElementFactory.make("fakesink", "fakesink")
            self.player.set_property("video-sink", fakesink)
            bus = self.player.get_bus()
            bus.add_signal_watch()
            bus.connect("message::eos", self.on_eos)
            bus.connect("message::error", self.on_error)
        self.player.connect("about-to-finish",  self.on_finished)

        self.moving_slider = False

    def on_error(self, bus, message):
        self.playing = PlayState.STOP
        self.player.set_state(Gst.State.NULL)
        err, debug = message.parse_error()
        print("Error: {}".format(err), debug)
        self.updateButtons()

    def on_eos(self, bus, message):
        self.playing = PlayState.STOP
        self.player.set_state(Gst.State.NULL)
        self.updateButtons()

    def on_finished(self, player):
        self.playing = PlayState.STOP
        self.slider.set_value(0)
        self.set_label(0)
        self.updateButtons()

    def play(self):
        self.playing = PlayState.PLAY
        uri = Liferea.enclosure_get_url(self.enclosures[self.pos])
        self.player.set_property("uri", uri)
        self.player.set_state(Gst.State.PLAYING)
        Liferea.ItemView.select_enclosure(self.pos)
        self.updateButtons()

        GObject.timeout_add(1000, self.updateSlider)

    def stop(self):
        self.playing = PlayState.STOP
        self.player.set_state(Gst.State.NULL)
        self.slider.set_value(0)
        self.updateButtons()

    def pause(self):
        self.playing = PlayState.PAUSE
        self.player.set_state(Gst.State.PAUSED)
        self.updateButtons()

    def playToggled(self, w):
        self.set_label(0)

        if(self.playing != PlayState.PLAY):
                self.play()
        else:
                self.pause()

    def next(self, w):
        self.stop()
        self.pos+=1
        self.play()

    def prev(self, w):
        self.stop()
        self.pos-=1
        self.play()

    def set_label(self, position):
        format = "%d:%02d"
        if self.moving_slider:
            format = "<i>%s</i>" % format
        self.label.set_markup (format % (position / 60, position % 60))

    def get_player_position(self):
        """Get the GStreamer player's position in nanoseconds"""
        try:
           if self.IS_GST010:
              return self.player.query_position(Gst.Format.TIME)[2]
           else:
              return self.player.query_position(Gst.Format.TIME)[1]
        except Exception as e:
            # pipeline must not be ready and does not know position
            print(e)
            return 0

    def get_player_duration(self):
        """Get the GStreamer player's duration in nanoseconds"""
        try:
            if self.IS_GST010:
                return self.player.query_duration(Gst.Format.TIME)[2]
            else:
                return self.player.query_duration(Gst.Format.TIME)[1]
        except Exception as e:
            # pipeline must not be ready and does not know position
            print(e)
            return 0

    def updateSlider(self):
        if self.playing != PlayState.PLAY or self.moving_slider:
            return False # cancel timeout

        duration = self.get_player_duration() / Gst.SECOND
        position = self.get_player_position() / Gst.SECOND
        self.slider.set_range(0, duration)
        self.slider.set_value(position)
        self.set_label(position)

        return True

    def updateButtons(self):
        Gtk.Widget.set_sensitive(self.prevButton, (self.pos != 0))
        Gtk.Widget.set_sensitive(self.nextButton, (len(self.enclosures) - 1 > self.pos))

        if(self.playing != PlayState.PLAY):
           self.playButtonImage.set_from_icon_name("media-playback-start", Gtk.IconSize.BUTTON)
        else:
           self.playButtonImage.set_from_icon_name("media-playback-pause", Gtk.IconSize.BUTTON)

    def on_slider_change_value(self, widget, scroll, value):
        self.set_label(value)
        self.move_to_nanosecs = value * Gst.SECOND

        return False

    def on_slider_button_press(self, widget, event):
        self.moving_slider = True

    def on_slider_button_release(self, widget, event):
        self.moving_slider = False

        # Stop when moving slider to very near the end
        end_cutoff = self.get_player_duration() - Gst.SECOND
        if self.move_to_nanosecs > end_cutoff and self.playing == PlayState.PLAY:
            self.playToggled(None)
        else:
            self.player.seek_simple(Gst.Format.TIME,
                                    Gst.SeekFlags.FLUSH |
                                    Gst.SeekFlags.KEY_UNIT,
                                    self.move_to_nanosecs)


    def do_load(self, parentWidget, enclosures):
        if parentWidget == None:
           print("ERROR: Could not find media player insertion widget!")

        # Test whether Media Player widget already exists
        childList = Gtk.Container.get_children(parentWidget)

        if len(childList) == 1:
           # We need to add a media player...
           vbox = Gtk.Box(Gtk.Orientation.HORIZONTAL, 0)
           vbox.props.margin = GMARGIN
           vbox.props.spacing = GMARGIN
           Gtk.Box.pack_start(parentWidget, vbox, True, True, 0);

           image = Gtk.Image()
           image.set_from_icon_name("media-skip-backward", Gtk.IconSize.BUTTON)
           self.prevButton = Gtk.Button.new()
           self.prevButton.add(image)
           self.prevButton.connect("clicked", self.prev)
           self.prevButton.set_direction(Gtk.TextDirection.LTR)
           Gtk.Box.pack_start(vbox, self.prevButton, False, False, 0)

           self.playButtonImage = Gtk.Image()
           self.playButtonImage.set_from_icon_name("media-playback-start", Gtk.IconSize.BUTTON)
           self.playButton = Gtk.Button.new()
           self.playButton.add(self.playButtonImage)
           self.playButton.connect("clicked", self.playToggled)
           self.playButton.set_direction(Gtk.TextDirection.LTR)
           Gtk.Box.pack_start(vbox, self.playButton, False, False, 0)

           image = Gtk.Image()
           image.set_from_icon_name("media-skip-forward", Gtk.IconSize.BUTTON)
           self.nextButton = Gtk.Button.new()
           self.nextButton.add(image)
           self.nextButton.connect("clicked", self.next)
           self.nextButton.set_direction(Gtk.TextDirection.LTR)
           Gtk.Box.pack_start(vbox, self.nextButton, False, False, 0)

           self.slider = Gtk.Scale(orientation = Gtk.Orientation.HORIZONTAL)
           self.slider.set_draw_value(False)
           self.slider.set_range(0, 100)
           self.slider.set_increments(1, 10)
           self.slider.connect("change-value", self.on_slider_change_value)
           self.slider.connect("button-press-event",
                               self.on_slider_button_press)
           self.slider.connect("button-release-event",
                               self.on_slider_button_release)
           self.slider.set_direction(Gtk.TextDirection.LTR)
           Gtk.Box.pack_start(vbox, self.slider, True, True, 0)

           self.label = Gtk.Label()
           self.set_label(0)
           Gtk.Box.pack_start(vbox, self.label, False, False, 0)

           Gtk.Widget.show_all(vbox)

        self.enclosures = enclosures
        self.pos = 0
        self.player.set_state(Gst.State.NULL)   # FIXME: Make this configurable?
        self.on_finished(self.player)

    #def do_activate(self):
        #print("=== MediaPlayer activate")

    def do_deactivate(self):
        window = self.object
