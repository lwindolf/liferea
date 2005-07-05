dnl Process this file with autoconf to produce a configure script.

AC_INIT
AC_CONFIG_SRCDIR([configure.ac])

AC_ENABLE_SHARED(yes)
AC_ENABLE_STATIC(no)

AM_INIT_AUTOMAKE(liferea, 0.9.3)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

dnl Work around libtool bug (fixed in the version 1.5a?)
AC_DEFUN([AC_PROVIDE_AC_LIBTOOL_DLOPEN], )
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_SYS_LARGEFILE

AC_ARG_ENABLE(sm,      AS_HELP_STRING([--disable-sm],[compile without X session management support]),,enable_sm=yes)
AC_ARG_ENABLE(dbus,    AS_HELP_STRING([--disable-dbus],[compile without DBUS support]),,enable_dbus=yes)
				   
AC_CHECK_FUNCS([strsep getaddrinfo])

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

dnl libcurl support
dnl AC_MSG_CHECKING([libcurl])
dnl if curl-config --libs > /dev/null 2>&1; then
dnl if false ; then
dnl 	AC_MSG_RESULT(yes)
dnl 	AM_CONDITIONAL(USE_LIBCURL, true)
dnl	CURL_CFLAGS="`curl-config --cflags`"
dnl	CURL_LIBS="`curl-config --libs`"
dnl	enable_libcurl=yes
dnl else
dnl	enable_libcurl=no
dnl	AC_MSG_RESULT(no)
dnl	AM_CONDITIONAL(USE_LIBCURL, false)
dnl fi

AC_SUBST(CURL_CFLAGS)
AC_SUBST(CURL_LIBS)


html_modules=

AC_MSG_CHECKING([for GtkHTML2 support])

PKG_CHECK_MODULES(GTKHTML2, libgtkhtml-2.0 >= 2.0.0,enable_gtkhtml2=yes,enable_gtkhtml2=no)

AC_SUBST(GTKHTML2_CFLAGS)
AC_SUBST(GTKHTML2_LIBS)AM_CONDITIONAL(WITH_GTKHTML2, test "x$enable_gtkhtml2" = "xyes")

dnl *******
dnl Mozilla
dnl *******

gecko_provider=mozilla

PKG_CHECK_MODULES(MOZILLA, $gecko_provider-gtkmozembed, enable_mozilla=yes, enable_mozilla=no)

# Check Firefox-mozilla
if test "x$enable_mozilla" = "xno"; then
   gecko_provider=firefox
   PKG_CHECK_MODULES(MOZILLA, $gecko_provider-gtkmozembed, enable_mozilla=yes, enable_mozilla=no)
fi

AM_CONDITIONAL(WITH_MOZILLA, test "x$enable_mozilla" = "xyes")
MOZILLA_LIB_DIR_SEARCH=

if test "x$enable_mozilla" = "xyes" ; then
   AC_MSG_CHECKING([for Mozilla support])

   AC_MSG_RESULT([$gecko_provider found!])

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
   MOZILLA_LIB_DIR_SEARCH="look_for_mozilla \"$MOZILLA_LIB_ROOT\""
   
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
   AC_SUBST(MOZILLA_LIB_DIR_SEARCH)
fi

dnl For now, require GtkHTML2 or Mozilla. GtkHTML3 should not be used!
if test "x$enable_mozilla" != "xyes" -a "x$enable_gtkhtml2" != "xyes" ; then
	AC_MSG_ERROR([
*** You must have either the GtkHTML2 or the Mozilla development
*** libraries installed in order to build Liferea!
])
fi

dnl Checking for gconftool-2
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

if test "x$GCONFTOOL" = xno; then
        AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

AM_GCONF_SOURCE_2

dnl hmm... I really have no clue which versions are neccessary!
pkg_modules="	gtk+-2.0 >= 2.4.0 
		gconf-2.0 >= 1.1.9 
		libxml-2.0 >= 2.4.1 
		gmodule-2.0 >= 2.0.0"

PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])

AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

uname=`uname`
AC_DEFINE_UNQUOTED(OS, $uname, [defines a OS version string, used for OS specific code])
AC_DEFINE_UNQUOTED(OSNAME, "$uname", [defines a OS version string, used for the user agent string])
AC_MSG_RESULT(user agent OS = $uname)

dnl Add the languages which your application supports here.
ALL_LINGUAS="bg cs de es fi fr it ja ko mk pl pt_BR ro ru sk zh_TW"
AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=liferea
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["${GETTEXT_PACKAGE}"], [gettext domain])


dnl ****
dnl DBUS
dnl ****

dbusversion=0

if test "x$enable_dbus" = "xyes"; then
   AC_MSG_NOTICE([checking for DBUS >= 0.30])
   PKG_CHECK_MODULES([DBUS], [dbus-1 >= 0.30 dbus-glib-1 >= 0.30],dbusversion=2,dbusversion=0)
   if test "x$dbusversion" = "x0"; then
      AC_MSG_NOTICE([checking for DBUS >= 0.23])
      PKG_CHECK_MODULES([DBUS], [dbus-1 >= 0.23 dbus-glib-1 >= 0.23],dbusversion=1,dbusversion=0)
   fi
   if test "x$dbusversion" != "x0" ; then
	  AC_DEFINE(USE_DBUS, 1, [Define if we're using DBUS.])
      enable_dbus=yes
   else
      enable_dbus=no
   fi
fi

AC_DEFINE_UNQUOTED(DBUS_VERSION, [[$dbusversion]], [Version of the DBUS API being used. See configure file for details])

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)
dnl AM_CONDITIONAL(WITH_DBUS, test "x$enable_dbus" != "xyes")

AC_CONFIG_FILES([
Makefile
src/Makefile
src/mozilla/Makefile
src/gtkhtml2/Makefile
src/net/Makefile
pixmaps/Makefile
opml/Makefile
po/Makefile.in
liferea.spec
liferea-SuSE.spec
src/liferea
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
echo "Build GtkHTML Plugin............ : $enable_gtkhtml2"
echo 
echo "Use X Session Management........ : $enable_sm"
echo "Use DBUS........................ : $enable_dbus"
echo
dnl echo Use libcurl download library.... : $enable_libcurl
dnl echo
eval eval echo Liferea will be installed in $bindir.
echo
echo configure complete, now type \'make\'
echo
