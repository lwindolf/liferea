dnl Process this file with autoconf to produce a configure script.

AC_INIT([liferea],[1.5.8-svn],[liferea-devel@lists.sourceforge.net])
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR([src/feedlist.c])

AC_ENABLE_SHARED(yes)
AC_ENABLE_STATIC(no)

AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AC_PREREQ(2.59)

LIFEREA_CONFIG_NICE(config.nice)

dnl Work around libtool bug (fixed in the version 1.5a?)
AC_DEFUN([AC_PROVIDE_AC_LIBTOOL_DLOPEN], )
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
IT_PROG_INTLTOOL([0.35.0])

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_SYS_LARGEFILE

AC_ARG_ENABLE(sm,        AS_HELP_STRING([--disable-sm],[compile without X session management support]),,enable_sm=yes)
AC_ARG_ENABLE(dbus,      AS_HELP_STRING([--disable-dbus],[compile without DBUS support]),,enable_dbus=yes)
AC_ARG_ENABLE(nm,        AS_HELP_STRING([--disable-nm],[compile without NetworkManager support]),,enable_nm=yes)
AC_ARG_ENABLE(libnotify, AS_HELP_STRING([--disable-libnotify],[don't compile the libnotify plugin]),,enable_libnotify=yes)
AC_ARG_ENABLE(lua,       AS_HELP_STRING([--disable-lua],[don't compile with LUA scripting support]),,enable_lua=yes)
AC_ARG_ENABLE(avahi,     AS_HELP_STRING([--disable-avahi],[don't compile with AVAHI support]),,enable_avahi=yes)

AC_MSG_CHECKING([for some Win32 platform])
case "$host" in
   *-*-cygwin*)
     platform_win32=yes
     SO_PREFIX=cyg
     SO_EXT=.dll
     ;;
   *-*-mingw*)
     platform_win32=yes
     SO_PREFIX=lib
     SO_EXT=.dll
     ;;
   *)
     platform_win32=no
     SO_PREFIX=lib
     SO_EXT=.so
     ;;
esac

AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL(PLATFORM_WIN32, test "$platform_win32" = "yes")
AC_SUBST(SO_PREFIX)
AC_SUBST(SO_EXT)
AC_DEFINE_UNQUOTED(LIBPREFIX, "$SO_PREFIX", [Platform dependent library prefix])

AC_CHECK_FUNCS([strsep fsync])

dnl Search for X libraries
AC_PATH_XTRA

dnl #######################################################################
dnl # Check for X session management libs
dnl #######################################################################

AC_MSG_CHECKING([for X11 session management library])

if test "x$enable_sm" = "xyes"; then
   enable_sm=no
   tmp=`echo "$X_PRE_LIBS" | grep -e "-lSM"`
   if test "x$tmp" != "x" ; then
		 enable_sm=yes
   fi 
else
     enable_sm=no
fi

if test "x$enable_sm" = "xyes"; then
   AC_DEFINE(USE_SM, 1, [Define if we're using X Session Management.])
   AC_MSG_RESULT(yes)
else
   AC_MSG_RESULT(no)
fi

PKG_PROG_PKG_CONFIG()

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

AM_CONDITIONAL(WITH_LIBNOTIFY, test "x$enable_libnotify" = "xyes")

dnl ***
dnl LUA
dnl ***

if test "x$enable_lua" = "xyes"; then
   dnl Lua 5.0 doesn't include any .pc file, so lua50.pc and lualib50.pc seems distribution specific (on my Gentoo, it is lua.pc)
   dnl Lua 5.1 does include a lua.pc file, so it shouldn't be distribution specific
   dnl However, the presence of lua.pc is certainly not a good indicator of lua version...
   PKG_CHECK_MODULES([LUA50], [lua50 lualib50] ,has_lua50=yes,has_lua50=no)
   PKG_CHECK_MODULES([LUA51], [lua] ,has_lua51=yes,has_lua51=no)
   
   if test "x$has_lua51" = "xyes" ; then
      LUA_CFLAGS="$LUA51_CFLAGS"
      LUA_LIBS="$LUA51_LIBS"
   elif test "x$has_lua50" = "xyes" ; then
      LUA_CFLAGS="$LUA50_CFLAGS"
      LUA_LIBS="$LUA50_LIBS"
   else
      enable_lua=no
   fi
   
   AC_SUBST(LUA_CFLAGS)
   AC_SUBST(LUA_LIBS)
else
   enable_lua=no
fi

AM_CONDITIONAL(WITH_LUA, test "x$enable_lua" = "xyes")

dnl *****
dnl AVAHI
dnl *****

if test "x$enable_avahi" = "xyes"; then
   PKG_CHECK_MODULES(AVAHI,
      avahi-client >= 0.6
      avahi-glib >= 0.6,
      enable_avahi=yes,
      enable_avahi=no)

   if test "x$enable_avahi" = "xyes"; then
      AC_DEFINE(USE_AVAHI, 1, [Define if we're using AVAHI.])
   fi      
   
   AC_SUBST(AVAHI_CFLAGS)
   AC_SUBST(AVAHI_LIBS)
else
   enable_avahi=no
fi

AM_CONDITIONAL(WITH_AVAHI, test "x$enable_avahi" = "xyes")

dnl Checking for gconftool-2
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

if test "x$GCONFTOOL" = xno; then
        AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

AM_GCONF_SOURCE_2

dnl Notes:
dnl
dnl  - libxml2/libxslt versions lower than the given ones are known
dnl    to crash on certain distributions, lower them on your own risk!
dnl  - exact minimum sqlite3 dependency is 3.3.9, but .pc only gives 3.3
dnl
pkg_modules="	gtk+-2.0 >= 2.10.0 
		glib-2.0 >= 2.16.0
		pango >= 1.4.0
		gconf-2.0 >= 1.1.9 
		libxml-2.0 >= 2.6.27
		libxslt >= 1.1.19
		sqlite3 >= 3.3
		gmodule-2.0 >= 2.0.0
		libglade-2.0 >= 2.0.0
		libcurl >= 7.15.0
		webkit-1.0"

PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])

AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

AC_MSG_CHECKING([for some Win32 platform])
case "$host" in
  *-*-cygwin*|*-*-mingw*)
    platform_win32=yes
    ;;
  *)
    platform_win32=no
    ;;
esac
AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL(PLATFORM_WIN32, test "$platform_win32" = "yes")

AC_MSG_CHECKING([for native Win32])
case "$host" in
  *-*-mingw*)
    os_win32=yes
    ;;
  *)
    os_win32=no
    ;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(OS_WIN32, test "$os_win32" = "yes")

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
src/glibcurl/Makefile
src/webkit/Makefile
src/parsers/Makefile
src/fl_sources/Makefile
src/notification/Makefile
src/scripting/Makefile
src/sync/Makefile
src/ui/Makefile
src/liferea
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
echo "AVAHI Support................... : $enable_avahi"
echo
eval eval echo Liferea will be installed in $bindir.
echo
echo configure complete, now type \'make\'
echo
