dnl Process this file with autoconf to produce a configure script.

AC_INIT([liferea],[1.7.0-svntrunk],[liferea-devel@lists.sourceforge.net])
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR([src/feedlist.c])

AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.6])

AC_PREREQ(2.59)

IT_PROG_INTLTOOL([0.35.0])

AC_PROG_CC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_SYS_LARGEFILE
AC_PROG_RANLIB

AC_ARG_ENABLE(sm,        AS_HELP_STRING([--disable-sm],[compile without X session management support]),,enable_sm=yes)
AC_ARG_ENABLE(dbus,      AS_HELP_STRING([--disable-dbus],[compile without DBUS support]),,enable_dbus=yes)
AC_ARG_ENABLE(nm,        AS_HELP_STRING([--disable-nm],[compile without NetworkManager support]),,enable_nm=yes)
AC_ARG_ENABLE(libnotify, AS_HELP_STRING([--disable-libnotify],[don't compile the libnotify plugin]),,enable_libnotify=yes)
AC_ARG_ENABLE(lua,       AS_HELP_STRING([--disable-lua],[don't compile with LUA scripting support]),,enable_lua=yes)
dnl AC_ARG_ENABLE(avahi,     AS_HELP_STRING([--disable-avahi],[don't compile with AVAHI support]),,enable_avahi=yes)

AC_CHECK_FUNCS([strsep])

PKG_PROG_PKG_CONFIG()

dnl #######################################################################
dnl # Check for X session management libs
dnl #######################################################################

if test "x$enable_sm" = "xyes"; then
  PKG_CHECK_MODULES([SM], [sm], [AC_DEFINE(USE_SM, 1, [Define if we're using X Session Management.])],[enable_sm=no])
fi

dnl *********
dnl libnotify
dnl *********

if test "x$enable_libnotify" = "xyes"; then
   PKG_CHECK_MODULES([LIBNOTIFY], libnotify >= 0.3.2,enable_libnotify=yes,enable_libnotify=no)
   AC_SUBST(LIBNOTIFY_CFLAGS)
   AC_SUBST(LIBNOTIFY_LIBS)
else
   enable_libnotify=no
fi

if test "x$enable_libnotify" = "xyes"; then
  AC_DEFINE(HAVE_LIBNOTIFY, 1, [Define if libnotify support is enabled])
fi

AM_CONDITIONAL(WITH_LIBNOTIFY, test "x$enable_libnotify" = "xyes")

dnl ***
dnl LUA
dnl ***

if test "x$enable_lua" = "xyes"; then
   PKG_CHECK_MODULES([LUA], [lua >= 5.1],, [AC_MSG_NOTICE([No lua.pc >= 5.1 present, is there a lua5.1.pc?])
     PKG_CHECK_MODULES([LUA], [lua5.1],,[enable_lua=no])])
   
   AC_SUBST(LUA_CFLAGS)
   AC_SUBST(LUA_LIBS)
fi

if test "x$enable_lua" = "xyes"; then
  AC_DEFINE(HAVE_LUA, 1, [Define if LUA support is enabled])
fi

AM_CONDITIONAL(WITH_LUA, test "x$enable_lua" = "xyes")

dnl *****
dnl AVAHI
dnl *****

dnl if test "x$enable_avahi" = "xyes"; then
dnl    PKG_CHECK_MODULES(AVAHI,
dnl       avahi-client >= 0.6
dnl       avahi-glib >= 0.6,
dnl       enable_avahi=yes,
dnl       enable_avahi=no)
dnl 
dnl    if test "x$enable_avahi" = "xyes"; then
dnl       AC_DEFINE(USE_AVAHI, 1, [Define if we're using AVAHI.])
dnl    fi      
dnl    
dnl    AC_SUBST(AVAHI_CFLAGS)
dnl    AC_SUBST(AVAHI_LIBS)
dnl else
dnl    enable_avahi=no
dnl fi
 
AM_CONDITIONAL(WITH_AVAHI, test "x$enable_avahi" = "xyes")

dnl Checking for gconftool-2
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

if test "x$GCONFTOOL" = xno; then
        AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

AM_GCONF_SOURCE_2

pkg_modules="	gtk+-2.0 >= 2.16.0 
		glib-2.0 >= 2.16.0
		pango >= 1.4.0
		gconf-2.0 >= 1.1.9 
		libxml-2.0 >= 2.6.27
		libxslt >= 1.1.19
		sqlite3 >= 3.6.10
		gmodule-2.0 >= 2.0.0
		libglade-2.0 >= 2.0.0
		libsoup-2.4 >= 2.26.1
		unique-1.0
		webkit-1.0 >= 1.1.7"

PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])

AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

uname=`uname`
AC_DEFINE_UNQUOTED(OS, $uname, [defines a OS version string, used for OS specific code])
AC_DEFINE_UNQUOTED(OSNAME, "$uname", [defines a OS version string, used for the user agent string])
AC_MSG_RESULT(user agent OS = $uname)

AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=liferea
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["${GETTEXT_PACKAGE}"], [gettext domain])


dnl ****
dnl DBUS
dnl ****


if test "x$enable_dbus" = "xyes"; then
   AC_MSG_NOTICE([checking for DBUS >= 0.30])
   PKG_CHECK_MODULES([DBUS], [dbus-1 >= 0.30 dbus-glib-1 >= 0.30],enable_dbus=yes,enable_dbus=no)
else
   enable_dbus=no
fi

if test "x$enable_dbus" = "xyes"; then
  AC_DEFINE(USE_DBUS, 1, [Define if we're using DBUS.])
fi

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)
AM_CONDITIONAL(WITH_DBUS, test "x$enable_dbus" = "xyes")

dnl **************
dnl NetworkManager
dnl **************

if test "x$enable_nm" = "xyes"; then
   AC_MSG_NOTICE([checking for NetworkManager])
   PKG_CHECK_MODULES([NM], [dbus-glib-1 libnm_glib],enable_nm=yes,enable_nm=no)
fi

if test "x$enable_nm" = "xyes"; then
  AC_DEFINE(USE_NM, 1, [Define if we're using NetworkManager.])
fi

AC_SUBST(NM_CFLAGS)
AC_SUBST(NM_LIBS)

AC_CONFIG_FILES([
Makefile
src/Makefile
src/webkit/Makefile
src/parsers/Makefile
src/fl_sources/Makefile
src/notification/Makefile
src/scripting/Makefile
src/sync/Makefile
src/ui/Makefile
doc/Makefile
doc/html/Makefile
xslt/Makefile
man/Makefile
man/pl/Makefile
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
liferea.spec
])
AC_OUTPUT

echo
echo "$PACKAGE $VERSION"
echo
echo "Build libnotify Plugin.......... : $enable_libnotify"
echo "Build LUA scripting Plugin ..... : $enable_lua"
echo
echo "Use X Session Management........ : $enable_sm"
echo "Use DBUS........................ : $enable_dbus"
echo "Use NetworkManager.............. : $enable_nm"
dnl echo "AVAHI Support................... : $enable_avahi"
echo
eval eval echo Liferea will be installed in $bindir.
echo
echo configure complete, now type \'make\'
echo
