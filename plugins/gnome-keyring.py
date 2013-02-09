from gi.repository import GObject
from gi.repository import Peas
from gi.repository import PeasGtk
from gi.repository import Gtk
from gi.repository import Liferea
from gi.repository import GnomeKeyring

class GnomeKeyringPlugin(GObject.Object, Liferea.AuthActivatable):
    __gtype_name__ = 'GnomeKeyringPlugin'

    object = GObject.property(type=GObject.Object)

    def do_activate(self):
	GnomeKeyring.unlock_sync("liferea", None)
	#
	# Dump all passwords for debugging:
	#
	#print "=== dump list on activate"
	#(result, ids) = GnomeKeyring.list_item_ids_sync("liferea")
        #for id in ids:	
        #   (result, item) = GnomeKeyring.item_get_info_sync("liferea", id)
        #   if result != GnomeKeyring.Result.OK:
	#      print '%s is locked!' % (id)
	#   else:
        #         print '%s = %s' % (item.get_display_name(), item.get_secret())
        #         self.attrs = GnomeKeyring.Attribute.list_new()
        #         result = GnomeKeyring.item_get_attributes_sync("liferea", id, self.attrs)
        #         for attr in GnomeKeyring.Attribute.list_to_glist(self.attrs):
        #            print '    %s => %s ' % (attr.name, attr.get_string())
	#print "=== dump list end"

    def do_deactivate(self):
        window = self.object

    def do_query(self, id):
        # Fetch secret by id
        attrs = GnomeKeyring.Attribute.list_new()
        GnomeKeyring.Attribute.list_append_string(attrs, 'id', id)
        result, value = GnomeKeyring.find_items_sync(GnomeKeyring.ItemType.GENERIC_SECRET, attrs)
        if result != GnomeKeyring.Result.OK:
          return

	#print 'password %s = %s' % (id, value[0].secret)
	#print 'password id = %s' % value[0].item_id

	username, password = value[0].secret.split('@@@')
  	Liferea.auth_info_from_store(id, username, password)

    def do_delete(self, id):
        keyring = GnomeKeyring.get_default_keyring_sync()[1]
        GnomeKeyring.item_delete_sync(keyring, id)

    def do_store(self, id, username, password):
	GnomeKeyring.create_sync("liferea", None)
        attrs = GnomeKeyring.Attribute.list_new()
        GnomeKeyring.Attribute.list_append_string(attrs, 'id', id)
        GnomeKeyring.Attribute.list_append_string(attrs, 'user', username)
	GnomeKeyring.item_create_sync("liferea", GnomeKeyring.ItemType.GENERIC_SECRET, repr(id), attrs, '@@@'.join([username, password]), True)

#class GnomeKeyringConfigurable(GObject.Object, PeasGtk.Configurable):
#    __gtype_name__ = 'GnomeKeyringConfigurable'
#
#    def do_create_configure_widget(self):
#        return Gtk.Label.new("GNOME Keyring configure widget")

