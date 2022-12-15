/**
 * @file main.c Liferea startup
 *
 * Copyright (C) 2003-2022 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 *
 * Some code like the command line handling was inspired by
 *
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "liferea_application.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include "conf.h"
#include "common.h"
#include "date.h"
#include "db.h"
#include "dbus.h"
#include "debug.h"
#include "feedlist.h"
#include "social.h"
#include "update.h"
#include "xml.h"
#include "ui/liferea_shell.h"

#include <girepository.h>

struct _LifereaApplication {
	GtkApplication	parent;
	gchar		*initialStateOption;
	gint		pluginsDisabled;
	LifereaDBus	*dbus;
	gulong		debug_flags;
};

G_DEFINE_TYPE (LifereaApplication, liferea_application, GTK_TYPE_APPLICATION)

static LifereaApplication *liferea_app = NULL;

static void
liferea_application_finalize (GObject *gobject)
{
	LifereaApplication *self = LIFEREA_APPLICATION(gobject);

	g_clear_object (&self->dbus);

	/* Chaining finalize from parent class. */
	G_OBJECT_CLASS(liferea_application_parent_class)->finalize(gobject);
}

/* GApplication "open" callback for receiving feed-add requests from remote --add-feed option or
 * adding feeds passed as argument. */
static void
on_app_open (GApplication *application,
             gpointer      files,
             gint          n_files,
             gchar        *hint,
             gpointer      user_data)
{
	int			i;
	GFile			**uris = (GFile **)files;
	GtkApplication		*gtk_app = GTK_APPLICATION (application);
	GList			*list = gtk_application_get_windows (gtk_app);
	LifereaApplication	*app = LIFEREA_APPLICATION (application);

	if (!list)
		liferea_shell_create (gtk_app, app->initialStateOption, app->pluginsDisabled);

	for (i=0;(i<n_files) && uris[i];i++) {
		gchar *uri = g_file_get_uri (uris[i]);

		/* When passed to GFile feeds using the feed scheme "feed:https://" become "feed:///https:/" */
		if (g_str_has_prefix (uri, "feed:///https:/")) {
			gchar *tmp = uri;
			uri = g_strdup_printf ("https://%s", uri + strlen ("feed:///https:/"));
			g_free (tmp);
		}
		feedlist_add_subscription (uri, NULL, NULL, 0);
		g_free (uri);
	}
}

/* GApplication "activate" callback emitted on the primary instance.  */
static void
on_app_activate (GtkApplication *gtk_app, gpointer user_data)
{
	gchar		*css_filename;
	GFile		*css_file;
	GtkCssProvider	*provider;
	GError		*error = NULL;
	GList		*list;
	LifereaApplication *app = LIFEREA_APPLICATION (gtk_app);

	list = gtk_application_get_windows (gtk_app);

	if (list) {
		liferea_shell_show_window ();
	} else {
		liferea_shell_create (gtk_app, app->initialStateOption, app->pluginsDisabled);
	}

	css_filename = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, "liferea.css", NULL);
	css_file = g_file_new_for_path (css_filename);
	provider = gtk_css_provider_new ();

	gtk_css_provider_load_from_file(provider, css_file, &error);

	if (G_UNLIKELY (!gtk_css_provider_load_from_file(provider,
							css_file,
							&error)))
	{
		g_critical ("Could not load CSS data: %s", error->message);
		g_clear_error (&error);
	} else {
		gtk_style_context_add_provider_for_screen (
				gdk_screen_get_default(),
				GTK_STYLE_PROVIDER (provider),
				GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	g_object_unref (provider);
	g_object_unref (css_file);
	g_free (css_filename);
}

/* Callback to the startup signal emitted only by the primary instance upon registration. */
static void
on_app_startup (GApplication *gapp, gpointer user_data)
{
	LifereaApplication *app = LIFEREA_APPLICATION (gapp);

	set_debug_level (app->debug_flags);

	/* Configuration necessary for network options, so it
	   has to be initialized before update_init() */
	conf_init ();

	/* We need to do the network initialization here to allow
	   network-manager to be setup before gtk_init() */
	update_init ();

	/* order is important! */
	date_init ();
	db_init ();
	xml_init ();
	social_init ();

	app->dbus = liferea_dbus_new ();
}

/* Callback to the "shutdown" signal emitted only on the primary instance; */
static void
on_app_shutdown (GApplication *app, gpointer user_data)
{
	GList *list;

	debug_enter ("liferea_shutdown");

	/* order is important ! */
	update_deinit ();

	/* When application is started as a service, it waits 10 seconds for a message.
	 * If no message arrives, it will shutdown without having created a window. */
	list = gtk_application_get_windows (GTK_APPLICATION (app));
	if (list) {
		liferea_shell_destroy ();
	}

	db_deinit ();
	social_free ();
	conf_deinit ();
	xml_deinit ();
	date_deinit ();

	debug_exit ("liferea_shutdown");
}

static void
show_version ()
{
	printf ("Liferea %s\n", VERSION);
}

static gboolean
debug_entries_parse_callback (const gchar *option_name,
			      const gchar *value,
			      gpointer data,
			      GError **error)
{
	gulong *debug_flags = data;

	if (g_str_equal (option_name, "--debug-all")) {
		*debug_flags = 0xffff - DEBUG_VERBOSE - DEBUG_TRACE;
	} else if (g_str_equal (option_name, "--debug-cache")) {
		*debug_flags |= DEBUG_CACHE;
	} else if (g_str_equal (option_name, "--debug-conf")) {
		*debug_flags |= DEBUG_CONF;
	} else if (g_str_equal (option_name, "--debug-db")) {
		*debug_flags |= DEBUG_DB;
	} else if (g_str_equal (option_name, "--debug-gui")) {
		*debug_flags |= DEBUG_GUI;
	} else if (g_str_equal (option_name, "--debug-html")) {
		*debug_flags |= DEBUG_HTML;
	} else if (g_str_equal (option_name, "--debug-net")) {
		*debug_flags |= DEBUG_NET;
	} else if (g_str_equal (option_name, "--debug-parsing")) {
		*debug_flags |= DEBUG_PARSING;
	} else if (g_str_equal (option_name, "--debug-performance")) {
		*debug_flags |= DEBUG_PERF;
	} else if (g_str_equal (option_name, "--debug-trace")) {
		*debug_flags |= DEBUG_TRACE;
	} else if (g_str_equal (option_name, "--debug-update")) {
		*debug_flags |= DEBUG_UPDATE;
	} else if (g_str_equal (option_name, "--debug-vfolder")) {
		*debug_flags |= DEBUG_VFOLDER;
	} else if (g_str_equal (option_name, "--debug-verbose")) {
		*debug_flags |= DEBUG_VERBOSE;
	} else {
		return FALSE;
	}

	return TRUE;
}

static gint
on_handle_local_options (GApplication *app, GVariantDict *options, gpointer user_data)
{
	gchar *uri;

	if (g_variant_dict_lookup_value (options, "version", NULL)) {
		show_version ();
		return 0; /* Show version and exit */
	}
	if (g_variant_dict_lookup (options, "add-feed", "s", &uri)) {
                GFile *uris[2];

                uris[0] = g_file_new_for_uri (uri);
                uris[1] = NULL;
		g_application_register (app, NULL, NULL);
                g_application_open (G_APPLICATION (app), uris, 1, "feed-add");

                g_object_unref (uris[0]);
		return -1;
	}
	return -1;
}

static void
liferea_application_class_init (LifereaApplicationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = liferea_application_finalize;
}

static void
liferea_application_init (LifereaApplication *self)
{
	GOptionGroup 	*debug;

	GOptionEntry entries[] = {
		{ "mainwindow-state", 'w', 0, G_OPTION_ARG_STRING, &self->initialStateOption, N_("Start Liferea with its main window in STATE. STATE may be `shown' or `hidden'"), N_("STATE") },
		{ "version", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, N_("Show version information and exit"), NULL },
		{ "add-feed", 'a', 0, G_OPTION_ARG_STRING, NULL, N_("Add a new subscription"), N_("uri") },
		{ "disable-plugins", 'p', 0, G_OPTION_FLAG_NONE, &self->pluginsDisabled, N_("Start with all plugins disabled"), NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	GOptionEntry debug_entries[] = {
		{ "debug-all", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all types"), NULL },
		{ "debug-cache", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages for the cache handling"), NULL },
		{ "debug-conf", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages for the configuration handling"), NULL },
		{ "debug-db", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of the database handling"), NULL },
		{ "debug-gui", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all GUI functions"), NULL },
		{ "debug-html", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Enables HTML rendering debugging. Each time Liferea renders HTML output it will also dump the generated HTML into ~/.cache/liferea/output.html"), NULL },
		{ "debug-net", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all network activity"), NULL },
		{ "debug-parsing", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all parsing functions"), NULL },
		{ "debug-performance", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages when a function takes too long to process"), NULL },
		{ "debug-trace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages when entering/leaving functions"), NULL },
		{ "debug-update", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of the feed update processing"), NULL },
		{ "debug-vfolder", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of the search folder matching"), NULL },
		{ "debug-verbose", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print verbose debugging messages"), NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	debug = g_option_group_new ("debug",
				    _("Print debugging messages for the given topic"),
				    _("Print debugging messages for the given topic"),
				    &self->debug_flags,
				    NULL);
	g_option_group_set_translation_domain(debug, GETTEXT_PACKAGE);
	g_option_group_add_entries (debug, debug_entries);

	g_application_add_main_option_entries (G_APPLICATION (self), entries);
	g_application_add_option_group (G_APPLICATION (self), debug);
	g_application_add_option_group (G_APPLICATION (self), g_irepository_get_option_group ());


	g_signal_connect (G_OBJECT (self), "activate", G_CALLBACK (on_app_activate), NULL);
	g_signal_connect (G_OBJECT (self), "open", G_CALLBACK (on_app_open), NULL);
	g_signal_connect (G_OBJECT (self), "shutdown", G_CALLBACK (on_app_shutdown), NULL);
	g_signal_connect (G_OBJECT (self), "startup", G_CALLBACK (on_app_startup), NULL);
	g_signal_connect (G_OBJECT (self), "handle-local-options", G_CALLBACK (on_handle_local_options), NULL);
}

static gboolean
liferea_application_shutdown_source_func (gpointer userdata)
{
	g_application_quit (G_APPLICATION (liferea_app));
	return FALSE;
}

void
liferea_application_shutdown (void)
{
	g_idle_add (liferea_application_shutdown_source_func, NULL);
}

gint
liferea_application_new (int argc, char *argv[])
{
	gint status;

	g_assert (NULL == liferea_app);

	liferea_app = g_object_new (LIFEREA_APPLICATION_TYPE,
		                    "flags", G_APPLICATION_HANDLES_OPEN,
		                    "application-id", "net.sourceforge.liferea",
		                    NULL);

	g_set_prgname ("liferea");
	g_set_application_name (_("Liferea"));
	gtk_window_set_default_icon_name ("net.sourceforge.liferea");	/* GTK theme support */
	status = g_application_run (G_APPLICATION (liferea_app), argc, argv);
	g_object_unref (liferea_app);

	return status;
}

void
liferea_application_rebuild_css(void)
{
	liferea_shell_rebuild_css ();
}
