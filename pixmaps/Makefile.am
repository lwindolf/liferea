SUBDIRS = 16x16 22x22 24x24 32x32 48x48 scalable
EXTRA_DIST = \
	mail-attachment.png	\
	default.svg	\
	folder.png   \
	emblem-important.png        \
	emblem-web.svg       \
	unread.png      \
	folder-saved-search.png     \
	fl_opml.png	\
	newsbin.png	

lifereadistpixdir = $(datadir)/$(PACKAGE)/pixmaps
lifereadistpix_DATA = $(EXTRA_DIST)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
	        echo "Updating Gtk icon cache."; \
	        $(gtk_update_icon_cache); \
	else \
	        echo "*** Icon cache not updated.  After install, run this:"; \
	        echo "***   $(gtk_update_icon_cache)"; \
	fi

uninstall-hook:
	@-if test -z "$(DESTDIR)"; then \
	        echo "Updating Gtk icon cache."; \
	        $(gtk_update_icon_cache); \
	fi
