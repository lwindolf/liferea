EXTRA_DIST = \
	attachment.png	\
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
	fl_opml.png	\
	fl_google.png	\
	fl_bloglines.png \
	newsbin.png	\
	comments.png	\
	bookmark.png	\
	tag.png		\
	grayflag.png

lifereadistpixdir = $(datadir)/$(PACKAGE)/pixmaps
lifereadistpix_DATA = $(EXTRA_DIST)

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
