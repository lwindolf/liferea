dnl Process this file with autoconf to produce a configure script.

AC_INIT([liferea],[1.16-RC3],[liferea-devel@lists.sourceforge.net])
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR([src/feedlist.c])

AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.11 foreign std-options -Wall -Werror])
AM_SILENT_RULES([yes])

dnl Needed for automake 1.12
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_PREREQ(2.59)

LT_INIT
IT_PROG_INTLTOOL([0.40.4])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_MAKE_SET
AC_SYS_LARGEFILE
GLIB_GSETTINGS

AC_CHECK_FUNCS([strsep])

PKG_PROG_PKG_CONFIG()

################################################################################
# Mandatory library dependencies

pkg_modules="	gtk+-3.0 >= 3.24.0
		glib-2.0 >= 2.68.0
		gio-2.0 >= 2.50.0
		pango >= 1.4.0 
		libxml-2.0 >= 2.6.27
		libxslt >= 1.1.19
		sqlite3 >= 3.7.0
		gmodule-2.0 >= 2.0.0
		gthread-2.0
		libsoup-3.0 >= 3.0.7
		webkit2gtk-4.1
		json-glib-1.0
		gobject-introspection-1.0
		gsettings-desktop-schemas
		libpeas-2
		fribidi >= 0.19.7"

################################################################################


PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])

AC_CHECK_LIB(glib-2.0, g_memdup2, [PACKAGE_CFLAGS="$PACKAGE_CFLAGS -DHAVE_G_MEMDUP2"])
AC_CHECK_LIB(glib-2.0, g_time_zone_new_identifier, [PACKAGE_CFLAGS="$PACKAGE_CFLAGS -DHAVE_G_TIME_ZONE_NEW_IDENTIFIER"])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)


PKG_CHECK_MODULES([WEB_EXTENSION], [
                  webkit2gtk-web-extension-4.1
                  ])
AC_SUBST([WEB_EXTENSION_CFLAGS])
AC_SUBST([WEB_EXTENSION_LIBS])

uname=`uname`
AC_DEFINE_UNQUOTED(OS, $uname, [defines a OS version string, used for OS specific code])
AC_DEFINE_UNQUOTED(OSNAME, "$uname", [defines a OS version string, used for the user agent string])
AC_MSG_RESULT(user agent OS = $uname)

################################################################################
# Plugins via Introspection

GOBJECT_INTROSPECTION_CHECK([0.9.3])

# Due to the mess in pygobject we need to know with which girepository we
# are run. Please change to 2.0 if building again pygobject > 3.52.3
# (See also https://github.com/lwindolf/liferea/issues/1425)
#
# At a later point the default here will switch to 2.0
AC_DEFINE_UNQUOTED(GI_REPOSITORY, "girepository-1.0", [defines the girepository version to use])

################################################################################

AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=liferea
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["${GETTEXT_PACKAGE}"], [gettext domain])


AC_CONFIG_FILES([
Makefile
net.sf.liferea.gschema.xml
src/Makefile
src/node_providers/Makefile
src/node_sources/Makefile
src/parsers/Makefile
src/plugins/Makefile
src/tests/Makefile
src/ui/Makefile
src/webkit/Makefile
src/webkit/web_extension/Makefile
doc/Makefile
doc/html/Makefile
man/Makefile
man/it/Makefile
pixmaps/Makefile
pixmaps/16x16/Makefile
pixmaps/22x22/Makefile
pixmaps/24x24/Makefile
pixmaps/32x32/Makefile
pixmaps/48x48/Makefile
pixmaps/scalable/Makefile
opml/Makefile
glade/Makefile
po/Makefile.in
src/liferea-add-feed
])
AC_OUTPUT

echo
echo "$PACKAGE $VERSION"
echo
eval eval echo Liferea will be installed in $bindir.
echo
echo configure complete, now type \'make\'
echo
