dnl Process this file with autoconf to produce a configure script.

AC_INIT([liferea],[1.2.9],[liferea-devel@lists.sourceforge.net])
AC_CANONICAL_TARGET([])
AC_CONFIG_SRCDIR([src/feedlist.c])

AC_ENABLE_SHARED(yes)
AC_ENABLE_STATIC(no)

AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_MAINTAINER_MODE

AC_PREREQ(2.59)

dnl Work around libtool bug (fixed in the version 1.5a?)
AC_DEFUN([AC_PROVIDE_AC_LIBTOOL_DLOPEN], )
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
IT_PROG_INTLTOOL([0.35.0])

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_SYS_LARGEFILE

AC_ARG_ENABLE(sm,        AS_HELP_STRING([--disable-sm],[compile without X session management support]),,enable_sm=yes)
AC_ARG_ENABLE(dbus,      AS_HELP_STRING([--disable-dbus],[compile without DBUS support]),,enable_dbus=yes)
AC_ARG_ENABLE(nm,        AS_HELP_STRING([--disable-nm],[compile without NetworkManager support]),,enable_nm=yes)
AC_ARG_ENABLE(gtkhtml2,  AS_HELP_STRING([--disable-gtkhtml2],[don't compile the gtkhtml2 backend]),,enable_gtkhtml2=yes)
AC_ARG_ENABLE(xulrunner, AS_HELP_STRING([--disable-xulrunner],[don't compile the xulrunner backend]),,enable_xulrunner=yes)
AC_ARG_ENABLE(gecko,     AS_HELP_STRING([--disable-gecko],[don't compile the Gecko backend]),,enable_gecko=yes)
AC_ARG_ENABLE(gecko,     AS_HELP_STRING([--enable-gecko=ARG],[specify which Gecko provider to use ("mozilla", "firefox" or "seamonkey")]),,enable_gecko=yes)
AC_ARG_ENABLE(gnutls,    AS_HELP_STRING([--enable-gnutls],[Attempt to use GNUTLS for SSL support @<:@default=yes@:>@]),,enable_gnutls=yes)
AC_ARG_ENABLE(libnotify, AS_HELP_STRING([--disable-libnotify],[don't compile the libnotify plugin]),,enable_libnotify=yes)
AC_ARG_ENABLE(lua,       AS_HELP_STRING([--disable-lua],[don't compile with LUA scripting support]),,enable_lua=yes)

AC_MSG_CHECKING([for 64bit platform])
case "$target" in
   x86_64-*-*)
     enable_gtkhtml2=no
     AC_MSG_RESULT([yes -> disabling GtkHTML2])
     ;;
   *)
     AC_MSG_RESULT([no])
     ;;
esac


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

AC_CHECK_FUNCS([strsep getaddrinfo fsync])

case "$host_os" in
        *solaris*)
                AC_CHECK_LIB(resolv, res_init)
                ;;
	*cygwin*)
		AC_CHECK_FUNC(res_init,,[AC_CHECK_LIB(resolv,res_init)])
		AC_CHECK_LIB(resolv,res_9_init)
		;;
esac

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

dnl *********
dnl XulRunner
dnl *********

if test "x$enable_xulrunner" = "xyes" ; then
   dnl AC_MSG_CHECKING([for XulRunner support])
   PKG_CHECK_MODULES(XULRUNNER, xulrunner-gtkmozembed, enable_xulrunner=yes, enable_xulrunner=no)
   XULRUNNER_HOME="`$PKG_CONFIG --variable=libdir xulrunner-gtkmozembed`"
   AC_SUBST([XULRUNNER_HOME])
   AC_SUBST(XULRUNNER_CFLAGS)
   AC_SUBST(XULRUNNER_LIBS)
   
   dnl the Mozilla XPCOM access means C++
   AC_PROG_CXX
fi

AM_CONDITIONAL(WITH_XULRUNNER, test "x$enable_xulrunner" = "xyes")

dnl ********
dnl GtkHTML2
dnl ********
 
if test "x$enable_gtkhtml2" = "xyes"; then
   dnl AC_MSG_CHECKING([for GtkHTML2 support])
   PKG_CHECK_MODULES([GTKHTML2], libgtkhtml-2.0 >= 2.0.0,enable_gtkhtml2=yes,enable_gtkhtml2=no)
   AC_SUBST(GTKHTML2_CFLAGS)
   AC_SUBST(GTKHTML2_LIBS)
else
   enable_gtkhtml2=no
fi

AM_CONDITIONAL(WITH_GTKHTML2, test "x$enable_gtkhtml2" = "xyes")

dnl *******
dnl Mozilla
dnl *******

if test "x$enable_gecko" = "xyes" -o "x$enable_gecko" = "xmozilla" ; then
   gecko_provider=mozilla
   PKG_CHECK_MODULES(MOZILLA, $gecko_provider-gtkmozembed, enable_mozilla=yes, enable_mozilla=no)
   AC_MSG_RESULT($gecko_provider: $enable_mozilla)
fi
if test "x$enable_mozilla" != "xyes" -a \( "x$enable_gecko" = "xyes" -o "x$enable_gecko" = "xfirefox" \) ; then
   gecko_provider=firefox
   PKG_CHECK_MODULES(MOZILLA, $gecko_provider-gtkmozembed, enable_mozilla=yes, enable_mozilla=no)
   AC_MSG_RESULT($gecko_provider: $enable_mozilla)
fi
if test "x$enable_mozilla" != "xyes" -a \( "x$enable_gecko" = "xyes" -o "x$enable_gecko" = "xseamonkey" \) ; then
   gecko_provider=seamonkey
   PKG_CHECK_MODULES(MOZILLA, $gecko_provider-gtkmozembed, enable_mozilla=yes, enable_mozilla=no)
   AC_MSG_RESULT($gecko_provider: $enable_mozilla)
fi

AM_CONDITIONAL(WITH_MOZILLA, test "x$enable_mozilla" = "xyes")

if test "x$enable_mozilla" = "xyes" ; then
   dnl AC_MSG_CHECKING([for Mozilla support])

   dnl AC_MSG_RESULT([$gecko_provider found!])

   dnl the Mozilla XPCOM access means C++
   AC_PROG_CXX
   
   dnl Note: with the infos from mozilla-gtkmozembed.pc
   dnl we got all includes and libraries for the C interface
   dnl to Mozilla, but not the XPCOM headers we also need.
   dnl Galeon's configure.in uses several hundred lines 
   dnl of macros to come around this. Which I would like to
   dnl avoid. So this might be not very portable...
   
   MOZILLA_INCLUDE_ROOT=`$PKG_CONFIG --cflags-only-I $gecko_provider-gtkmozembed | awk '{print $1}' | sed "s/^-I//"`
   MOZILLA_INCLUDE_ROOT=`dirname $MOZILLA_INCLUDE_ROOT`
   MOZILLA_LIB_ROOT=`$PKG_CONFIG --libs-only-L $gecko_provider-gtkmozembed | awk '{print $1}' |  cut -c 3-`
   
   dnl Maybe we don't need all of this...
   MOZILLA_CFLAGS="$MOZILLA_CFLAGS \
-I$MOZILLA_INCLUDE_ROOT \
-I$MOZILLA_INCLUDE_ROOT/appcomps \
-I$MOZILLA_INCLUDE_ROOT/caps \
-I$MOZILLA_INCLUDE_ROOT/content \
-I$MOZILLA_INCLUDE_ROOT/cookie \
-I$MOZILLA_INCLUDE_ROOT/docshell \
-I$MOZILLA_INCLUDE_ROOT/dom \
-I$MOZILLA_INCLUDE_ROOT/exthandler \
-I$MOZILLA_INCLUDE_ROOT/find \
-I$MOZILLA_INCLUDE_ROOT/gfx \
-I$MOZILLA_INCLUDE_ROOT/gtkembedmoz \
-I$MOZILLA_INCLUDE_ROOT/helperAppDlg \
-I$MOZILLA_INCLUDE_ROOT/history \
-I$MOZILLA_INCLUDE_ROOT/java \
-I$MOZILLA_INCLUDE_ROOT/js \
-I$MOZILLA_INCLUDE_ROOT/jsconsole \
-I$MOZILLA_INCLUDE_ROOT/layout \
-I$MOZILLA_INCLUDE_ROOT/mimetype \
-I$MOZILLA_INCLUDE_ROOT/mozxfer \
-I$MOZILLA_INCLUDE_ROOT/necko \
-I$MOZILLA_INCLUDE_ROOT/necko2 \
-I$MOZILLA_INCLUDE_ROOT/nkcache \
-I$MOZILLA_INCLUDE_ROOT/oji \
-I$MOZILLA_INCLUDE_ROOT/pref \
-I$MOZILLA_INCLUDE_ROOT/progressDlg \
-I$MOZILLA_INCLUDE_ROOT/sidebar \
-I$MOZILLA_INCLUDE_ROOT/string \
-I$MOZILLA_INCLUDE_ROOT/shistory \
-I$MOZILLA_INCLUDE_ROOT/typeaheadfind \
-I$MOZILLA_INCLUDE_ROOT/uconv \
-I$MOZILLA_INCLUDE_ROOT/uriloader \
-I$MOZILLA_INCLUDE_ROOT/wallet \
-I$MOZILLA_INCLUDE_ROOT/webbrowserpersist \
-I$MOZILLA_INCLUDE_ROOT/webbrwsr \
-I$MOZILLA_INCLUDE_ROOT/webshell \
-I$MOZILLA_INCLUDE_ROOT/widget \
-I$MOZILLA_INCLUDE_ROOT/windowwatcher \
-I$MOZILLA_INCLUDE_ROOT/xpcom \
-I$MOZILLA_INCLUDE_ROOT/xpconnect"

   AC_SUBST(MOZILLA_CFLAGS)
   AC_SUBST(MOZILLA_LIBS)   
   AC_SUBST(MOZILLA_LIB_ROOT)
   AC_DEFINE_UNQUOTED([MOZILLA_LIB_ROOT], ["${MOZILLA_LIB_ROOT}"], [Mozilla component path])
else
   enable_mozilla=no
fi

dnl For now, require GtkHTML2, Mozilla or XulRunner.
if test "x$enable_mozilla" != "xyes" -a "x$enable_gtkhtml2" != "xyes" -a "x$enable_xulrunner" != "xyes" ; then
	AC_MSG_ERROR([
*** You must have either the GtkHTML2, XulRunner or the Mozilla 
*** development libraries installed in order to build Liferea!
])
fi

dnl Checking for gconftool-2
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

if test "x$GCONFTOOL" = xno; then
        AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

AM_GCONF_SOURCE_2

pkg_modules="	gtk+-2.0 >= 2.8.0 
		pango >= 1.4.0
		gconf-2.0 >= 1.1.9 
		libxml-2.0 >= 2.6.27
		libxslt >= 1.1.19
		gmodule-2.0 >= 2.0.0"

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
fi

if test "x$enable_dbus" = "xyes"; then
  AC_DEFINE(USE_DBUS, 1, [Define if we're using DBUS.])
fi

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)
dnl AM_CONDITIONAL(WITH_DBUS, test "x$enable_dbus" != "xyes")

dnl ****
dnl NetworkManager
dnl ****
if test "x$enable_nm" = "xyes"; then
   AC_MSG_NOTICE([checking for NetworkManager])
   PKG_CHECK_MODULES([NM], [dbus-glib-1 libnm_glib],enable_nm=yes,enable_nm=no)
fi

if test "x$enable_nm" = "xyes"; then
  AC_DEFINE(USE_NM, 1, [Define if we're using NetworkManager.])
fi

AC_SUBST(NM_CFLAGS)
AC_SUBST(NM_LIBS)

dnl #
dnl # Check for GNUTLS if it's specified.
dnl #
if test "x$enable_gnutls" != "xno"; then
        enable_gnutls="no"
        prefix=`eval echo $prefix`

        AC_ARG_WITH(gnutls-includes,
                [  --with-gnutls-includes=PREFIX   Location of GNUTLS includes.],
                [ with_gnutls_includes="$withval" ],
                [ with_gnutls_includes="$prefix/include" ])

        have_gnutls_includes="no"

        if test "x$with_gnutls_includes" != "xno"; then
                CPPFLAGS_save="$CPPFLAGS"

                AC_MSG_CHECKING(for GNUTLS includes)
                AC_MSG_RESULT("")

                CPPFLAGS="$CPPFLAGS -I$with_gnutls_includes"
                AC_CHECK_HEADERS(gnutls/gnutls.h, [ gnutls_includes="yes" ])
                CPPFLAGS="$CPPFLAGS_save"

                if test "x$gnutls_includes" != "xno" -a \
                        "x$gnutls_includes" != "x"; then
                        have_gnutls_includes="yes"

                        GNUTLS_CFLAGS="-I$with_gnutls_includes"
                else
                        GNUTLS_CFLAGS=""
                fi
        else
                AC_MSG_CHECKING(for GNUTLS includes)
                AC_MSG_RESULT(no)
        fi

        AC_ARG_WITH(gnutls-libs,
                [  --with-gnutls-libs=PREFIX       Location of GNUTLS libraries.],
                [ with_gnutls_libs="$withval" ])

        if test "x$with_gnutls_libs"     != "xno" -a \
                "x$have_gnutls_includes" != "xno"; then

                LDFLAGS_save="$LDFLAGS"
                LIBS_save="$LIBS"

                case $with_gnutls_libs in
                        ""|-L*) ;;
                        *) with_gnutls_libs="-L$with_gnutls_libs" ;;
                esac

                AC_CACHE_CHECK([for GNUTLS libraries], gnutls_libs,
                [
                        LDFLAGS="$LDFLAGS $with_gnutls_libs"
                        LIBS="-lgnutls -lgcrypt"
                        AC_TRY_LINK_FUNC(gnutls_init, gnutls_libs="yes", gnutls_libs="no")
                        LDFLAGS="$LDFLAGS_save"
                        LIBS="$LIBS_save"
                ])

                if test "x$gnutls_libs" != "xno"; then
                        AC_DEFINE(HAVE_GNUTLS, 1, [Define if you have GNUTLS])
                        msg_gnutls="GNUTLS"
                        GNUTLS_LIBS="$with_gnutls_libs -lgnutls -lgcrypt"

                        enable_gnutls="yes"
                else
                        GNUTLS_CFLAGS=""
                        GNUTLS_LIBS=""
                fi
        else
                AC_MSG_CHECKING(for GNUTLS libraries)
                AC_MSG_RESULT(no)
        fi
else
        GNUTLS_CFLAGS=""
        GNUTLS_LIBS=""
fi

AC_SUBST(GNUTLS_CFLAGS)
AC_SUBST(GNUTLS_LIBS)

AM_CONDITIONAL(USE_GNUTLS, test "x$enable_gnutls" = "xyes")


AC_CONFIG_FILES([
Makefile
src/Makefile
src/gtkhtml2/Makefile
src/mozilla/Makefile
src/net/Makefile
src/parsers/Makefile
src/fl_sources/Makefile
src/notification/Makefile
src/scripting/Makefile
src/ui/Makefile
src/liferea
doc/Makefile
doc/html/Makefile
xslt/Makefile
man/Makefile
man/pl/Makefile
pixmaps/Makefile
opml/Makefile
po/Makefile.in
liferea.spec
])
AC_OUTPUT

echo
echo "$PACKAGE $VERSION"
echo
if test "x$enable_mozilla" = "xyes"; then
   echo "Build Mozilla Plugin............ : $enable_mozilla ($gecko_provider)"
else
   echo "Build Mozilla Plugin............ : $enable_mozilla"
fi
echo "Build XulRunner Plugin.......... : $enable_xulrunner"
echo "Build GtkHTML Plugin............ : $enable_gtkhtml2"
echo 
echo "Build libnotify Plugin.......... : $enable_libnotify"
echo "Build LUA scripting Plugin ..... : $enable_lua"
echo
echo "Use X Session Management........ : $enable_sm"
echo "Use DBUS........................ : $enable_dbus"
echo "Use NetworkManager.............. : $enable_nm"
echo "GNUTLS SSL/HTTPS Support........ : $enable_gnutls"
echo
eval eval echo Liferea will be installed in $bindir.
echo
echo configure complete, now type \'make\'
echo
