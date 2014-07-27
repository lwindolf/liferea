## Process this file with automake to produce Makefile.in

SUBDIRS = doc man opml pixmaps po src xslt glade

desktop_in_files = liferea.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

appdatadir = $(datadir)/appdata
appdata_in_files = $(PACKAGE_TARNAME).appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

cssdir = $(pkgdatadir)/css
css_DATA = \
	css/liferea.css \
	css/user.css \
	css/adblock.css

dtddir = $(pkgdatadir)/dtd
dtd_DATA = dtd/html.ent

plugindir = $(pkglibdir)/plugins
plugin_DATA = \
	plugins/gnome-keyring.py \
	plugins/gnome-keyring.plugin \
	plugins/libnotify.py \
	plugins/libnotify.plugin \
	plugins/media-player.py \
	plugins/media-player.plugin \
	plugins/trayicon.py \
	plugins/trayicon.plugin

gsettings_SCHEMAS = net.sf.liferea.gschema.xml
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

data_convertdir = $(datadir)/GConf/gsettings
dist_data_convert_DATA = liferea.convert

EXTRA_DIST = \
	net.sf.liferea.gschema.xml.in \
	po/liferea.pot \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(schema_DATA) \
	$(css_DATA) \
	$(dtd_DATA) \
	$(plugin_DATA) \
	$(gsettings_SCHEMAS) \
	$(appdata_in_files) \
	$(appdata_DATA)

DISTCLEANFILES = \
	liferea.desktop \
	intltool-extract \
	intltool-merge \
	intltool-update \
	$(gsettings_SCHEMAS)

CLEANFILES = \
	$(gsettings_SCHEMAS) \
	$(appdata_DATA) 

po/liferea.pot:
	cd po && $(MAKE) liferea.pot
