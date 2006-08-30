EXTRA_DIST = \
	available.png   \
	available_offline.png \
	default.png	\
	directory.png   \
	empty.png       \
	empty_offline.png \
	flag.png        \
	ocs.png         \
	read.xpm        \
	unread.png      \
	online.png	\
	offline.png	\
	vfolder.png     \
	edit.png	\
	technorati.png	\
	arrow.png	\
	liferea.png	\
	fl_opml.png

lifereadistpixdir = $(datadir)/$(PACKAGE)/pixmaps
lifereadistpix_DATA = available.png  directory.png  empty.png  flag.png  \
	default.png ocs.png read.xpm unread.png online.png offline.png vfolder.png \
	edit.png technorati.png arrow.png liferea.png \
	available_offline.png empty_offline.png \
	fl_opml.png

distpixmapdir = $(datadir)/icons/hicolor/48x48/apps
distpixmap_DATA = liferea.png

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
	        echo "Updating Gtk icon cache."; \
	        $(gtk_update_icon_cache); \
	else \
	        echo "*** Icon cache not updated.  After install, run this:"; \
	        echo "***   $(gtk_update_icon_cache)"; \
	fi
