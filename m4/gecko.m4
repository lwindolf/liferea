# Copyright © 2000-2004 Marco Pesenti Gritti
# Copyright © 2003, 2004, 2005, 2006 Christian Persch
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

# GECKO_INIT(VARIABLE,[ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
#
# Checks for gecko, and aborts if it's not found
#
# Checks for -fshort-wchar compiler variable, and adds it to
# AM_CXXFLAGS if found
#
# Checks whether RTTI is enabled, and adds -fno-rtti to 
# AM_CXXFLAGS otherwise
#
# Checks whether the gecko build is a debug build, and adds
# debug flags to AM_CXXFLAGS if it is.
#
# Expanded variables:
# VARIABLE: Which gecko was found (e.g. "xulrunnner", "seamonkey", ...)
# VARIABLE_FLAVOUR: The flavour of the gecko that was found
# VARIABLE_HOME:
# VARIABLE_NSPR: set if nspr is provided by gecko flags
# VARIABLE_PREFIX:
# VARIABLE_INCLUDE_ROOT:
# VARIABLE_VERSION: The version of the gecko that was found
# VARIABLE_VERSION:
# VARIABLE_VERSION_INT:

AC_DEFUN([GECKO_INIT],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_REQUIRE([AC_PROG_AWK])dnl

AC_PROG_AWK

# ************************
# Check which gecko to use
# ************************

AC_MSG_CHECKING([which gecko to use])

AC_ARG_WITH([gecko],
	AS_HELP_STRING([--with-gecko@<:@=mozilla|firefox|seamonkey|xulrunner|libxul-embedding|libxul@:>@],
		       [Which gecko engine to use (autodetected by default)]))

# Backward compat
AC_ARG_WITH([mozilla],[],[with_gecko=$withval],[])

gecko_cv_gecko=$with_gecko

# Autodetect gecko
_geckos="xulrunner firefox mozilla-firefox seamonkey mozilla libxul-embedding libxul"
if test -z "$gecko_cv_gecko"; then
	for lizard in $_geckos; do
		if $PKG_CONFIG --exists $lizard-xpcom; then
			gecko_cv_gecko=$lizard
			break;
		elif $PKG_CONFIG --exists $lizard-unstable; then
			gecko_cv_gecko=$lizard
			break;
		fi
	done
fi

AC_MSG_RESULT([$gecko_cv_gecko])

if test "x$gecko_cv_gecko" = "x"; then
	ifelse([$3],,[AC_MSG_ERROR([No gecko found; you may need to adjust PKG_CONFIG_PATH or install a mozilla/firefox/xulrunner -devel package])],[$3])
	gecko_cv_have_gecko=no
elif ! ( echo "$_geckos" | egrep "(^| )$gecko_cv_gecko(\$| )" > /dev/null); then
	AC_MSG_ERROR([Unknown gecko "$gecko_cv_gecko" specified])
else
	ifelse([$2],,[],[$2])
	gecko_cv_have_gecko=yes
fi

AC_MSG_CHECKING([manual gecko home set])

AC_ARG_WITH([gecko-home],
	AS_HELP_STRING([--with-gecko-home@<:@=[path]@:>@],
		       [Manually set MOZILLA_FIVE_HOME]))

gecko_cv_gecko_home=$with_gecko_home

# ****************
# Define variables
# ****************

if test "$gecko_cv_have_gecko" = "yes"; then

case "$gecko_cv_gecko" in
mozilla) gecko_cv_gecko_flavour=mozilla ;;
seamonkey) gecko_cv_gecko_flavour=mozilla ;;
*firefox) gecko_cv_gecko_flavour=toolkit ;;
xulrunner) gecko_cv_gecko_flavour=toolkit ;;
libxul*) gecko_cv_gecko_flavour=toolkit ;;
esac

if $PKG_CONFIG --exists  ${gecko_cv_gecko}-xpcom; then
	_GECKO_INCLUDE_ROOT="`$PKG_CONFIG --variable=includedir ${gecko_cv_gecko}-xpcom`"
	_GECKO_CFLAGS="-I$_GECKO_INCLUDE_ROOT"
	_GECKO_LIBDIR="`$PKG_CONFIG --variable=libdir ${gecko_cv_gecko}-xpcom`"
	_GECKO_HOME="`$PKG_CONFIG --variable=libdir ${gecko_cv_gecko}-xpcom`"
	_GECKO_PREFIX="`$PKG_CONFIG --variable=prefix ${gecko_cv_gecko}-xpcom`"
	_GECKO_NSPR=no # XXX asac: this is currently a blind guess and should be a AC test
else
	_GECKO_INCLUDE_ROOT="`$PKG_CONFIG --variable=includedir ${gecko_cv_gecko}`/unstable"
	_GECKO_CFLAGS="`$PKG_CONFIG --cflags ${gecko_cv_gecko}` `$PKG_CONFIG --cflags ${gecko_cv_gecko}-unstable`"
	_GECKO_LIBDIR="`$PKG_CONFIG --variable=sdkdir ${gecko_cv_gecko}`/bin"
	_GECKO_HOME=$with_gecko_home
	_GECKO_PREFIX="`$PKG_CONFIG --variable=prefix ${gecko_cv_gecko}`"
	_GECKO_NSPR=no # XXX asac: this is currently a blind guess and should be a AC test
fi
fi # if gecko_cv_have_gecko

if test "$gecko_cv_gecko_flavour" = "toolkit"; then
	AC_DEFINE([HAVE_MOZILLA_TOOLKIT],[1],[Define if mozilla is of the toolkit flavour])
fi

$1[]=$gecko_cv_gecko
$1[]_FLAVOUR=$gecko_cv_gecko_flavour
$1[]_INCLUDE_ROOT=$_GECKO_INCLUDE_ROOT
$1[]_CFLAGS=$_GECKO_CFLAGS
$1[]_LIBDIR=$_GECKO_LIBDIR
$1[]_HOME=$_GECKO_HOME
$1[]_PREFIX=$_GECKO_PREFIX
$1[]_NSPR=$_GECKO_NSPR

# **************************************************************
# This is really gcc-only
# Do this test using CXX only since some versions of gcc
# 2.95-2.97 have a signed wchar_t in c++ only and some versions
# only have short-wchar support for c++.
# **************************************************************

_GECKO_EXTRA_CPPFLAGS=
_GECKO_EXTRA_CFLAGS=
_GECKO_EXTRA_CXXFLAGS=
_GECKO_EXTRA_LDFLAGS=

if test "$gecko_cv_have_gecko" = "yes"; then

AC_LANG_PUSH([C++])

_SAVE_CXXFLAGS=$CXXFLAGS
CXXFLAGS="$CXXFLAGS $_GECKO_EXTRA_CXXFLAGS -fshort-wchar"

AC_CACHE_CHECK([for compiler -fshort-wchar option],
	gecko_cv_have_usable_wchar_option,
	[AC_RUN_IFELSE([AC_LANG_SOURCE(
		[[#include <stddef.h>
		  int main () {
		    return (sizeof(wchar_t) != 2) || (wchar_t)-1 < (wchar_t) 0 ;
		  } ]])],
		[gecko_cv_have_usable_wchar_option="yes"],
		[gecko_cv_have_usable_wchar_option="no"],
		[gecko_cv_have_usable_wchar_option="maybe (cross-compiling)"])])

CXXFLAGS="$_SAVE_CXXFLAGS"

AC_LANG_POP([C++])

if test "$gecko_cv_have_usable_wchar_option" = "yes"; then
	_GECKO_EXTRA_CXXFLAGS="-fshort-wchar"
	AM_CXXFLAGS="$AM_CXXFLAGS -fshort-wchar"
fi

fi # if gecko_cv_have_gecko

# **************
# Check for RTTI
# **************

if test "$gecko_cv_have_gecko" = "yes"; then

AC_MSG_CHECKING([whether to enable C++ RTTI])
AC_ARG_ENABLE([cpp-rtti],
	AS_HELP_STRING([--enable-cpp-rtti],[Enable C++ RTTI]),
	[],[enable_cpp_rtti=no])
AC_MSG_RESULT([$enable_cpp_rtti])

if test "$enable_cpp_rtti" = "no"; then
	_GECKO_EXTRA_CXXFLAGS="-fno-rtti $_GECKO_EXTRA_CXXFLAGS"
	AM_CXXFLAGS="-fno-rtti $AM_CXXFLAGS"
fi

fi # if gecko_cv_have_gecko

# *************
# Various tests
# *************

if test "$gecko_cv_have_gecko" = "yes"; then

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $_GECKO_EXTRA_CPPFLAGS $_GECKO_CFLAGS"

AC_MSG_CHECKING([[whether we have a gtk 2 gecko build]])
AC_RUN_IFELSE(
	[AC_LANG_SOURCE(
		[[#include <mozilla-config.h>
		  #include <string.h>
		  #include <stdlib.h>
                  int main(void) {
		    if (strcmp (MOZ_DEFAULT_TOOLKIT, "gtk2") == 0 ||
		        strcmp (MOZ_DEFAULT_TOOLKIT, "cairo-gtk2") == 0)
			    return EXIT_SUCCESS;
		
		    return EXIT_FAILURE;
		  } ]]
	)],
	[result=yes],
	[AC_MSG_ERROR([[This program needs a gtk 2 gecko build]])],
        [result=maybe])
AC_MSG_RESULT([$result])

AC_MSG_CHECKING([[whether we have a gecko debug build]])
AC_COMPILE_IFELSE(
	[AC_LANG_SOURCE(
		[[#include <mozilla-config.h>
		  #if !defined(MOZ_REFLOW_PERF) || !defined(MOZ_REFLOW_PERF_DSP)
		  #error No
		  #endif]]
	)],
	[gecko_cv_have_debug=yes],
	[gecko_cv_have_debug=no])
AC_MSG_RESULT([$gecko_cv_have_debug])

AC_MSG_CHECKING([[whether we have a xpcom glue]])
AC_COMPILE_IFELSE(
	[AC_LANG_SOURCE(
		[[
		  #ifndef XPCOM_GLUE
		  #error "no xpcom glue found"
		  #endif]]
	)],
	[gecko_cv_have_xpcom_glue=yes],
	[gecko_cv_have_xpcom_glue=no])
AC_MSG_RESULT([$gecko_cv_have_xpcom_glue])

CPPFLAGS="$_SAVE_CPPFLAGS"

AC_LANG_POP([C++])

if test "$gecko_cv_have_debug" = "yes"; then
	_GECKO_EXTRA_CXXFLAGS="$_GECKO_EXTRA_CXXFLAGS -DDEBUG -D_DEBUG"
	AM_CXXFLAGS="-DDEBUG -D_DEBUG $AM_CXXFLAGS"

	AC_DEFINE([HAVE_GECKO_DEBUG],[1],[Define if gecko is a debug build])
fi

if test "$gecko_cv_have_xpcom_glue" = "yes"; then
	AC_DEFINE([HAVE_GECKO_XPCOM_GLUE],[1],[Define if xpcom glue is used])
fi

fi # if gecko_cv_have_gecko

# ***********************
# Check for gecko version
# ***********************

if test "$gecko_cv_have_gecko" = "yes"; then

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $_GECKO_CFLAGS"

AC_CACHE_CHECK([for gecko version],
	[gecko_cv_gecko_version],
	[AC_RUN_IFELSE(
		[AC_LANG_PROGRAM([[
#include <mozilla-config.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
]],[[
FILE *stream;
const char *version = "";

if (!setlocale (LC_ALL, "C")) return 127;

stream = fopen ("conftest.data", "w");
if (!stream) return 126;

#ifdef MOZILLA_1_8_BRANCH
version = "1.8.1";
#else
if (strncmp (MOZILLA_VERSION, "1.9", strlen ("1.9")) == 0) {
	version = "1.9";
} else if (strncmp (MOZILLA_VERSION, "1.8", strlen ("1.8")) == 0) {
	version = "1.8";
} else {
	version = "1.7";
}
#endif
fprintf (stream, "%s\n", version);
if (fclose (stream) != 0) return 125;

return EXIT_SUCCESS;
]])],
	[gecko_cv_gecko_version="$(cat conftest.data)"],
	[AC_MSG_FAILURE([could not determine gecko version])],
	[gecko_cv_gecko_version="1.7"])
])

CPPFLAGS="$_SAVE_CPPFLAGS"

AC_LANG_POP([C++])

gecko_cv_gecko_version_int="$(echo "$gecko_cv_gecko_version" | $AWK -F . '{print [$]1 * 1000000 + [$]2 * 1000 + [$]3}')"

if test "$gecko_cv_gecko_version_int" -lt "1007000" -o "$gecko_cv_gecko_version_int" -gt "1009000"; then
	AC_MSG_ERROR([Gecko version $gecko_cv_gecko_version is not supported!])
fi

if test "$gecko_cv_gecko_version_int" -ge "1007000"; then
	AC_DEFINE([HAVE_GECKO_1_7],[1],[Define if we have gecko 1.7])
	gecko_cv_have_gecko_1_7=yes
fi
if test "$gecko_cv_gecko_version_int" -ge "1008000"; then
	AC_DEFINE([HAVE_GECKO_1_8],[1],[Define if we have gecko 1.8])
	gecko_cv_have_gecko_1_8=yes
fi
if test "$gecko_cv_gecko_version_int" -ge "1008001"; then
	AC_DEFINE([HAVE_GECKO_1_8_1],[1],[Define if we have gecko 1.8.1])
	gecko_cv_have_gecko_1_8_1=yes
fi
if test "$gecko_cv_gecko_version_int" -ge "1009000"; then
	AC_DEFINE([HAVE_GECKO_1_9],[1],[Define if we have gecko 1.9])
	gecko_cv_have_gecko_1_9=yes
fi

fi # if gecko_cv_have_gecko

$1[]_VERSION=$gecko_cv_gecko_version
$1[]_VERSION_INT=$gecko_cv_gecko_version_int

# **************************************************
# Packages that we need to check for with pkg-config 
# **************************************************

gecko_cv_extra_libs=
gecko_cv_glue_libs=
gecko_cv_extra_pkg_dependencies=

if test "$gecko_cv_gecko_version_int" -ge "1009000"; then
	if ! test "$gecko_cv_have_xpcom_glue" = "yes"; then
		gecko_cv_extra_libs="-L$_GECKO_LIBDIR -lxul"
	else
		gecko_cv_glue_libs="-L$_GECKO_LIBDIR -lxpcomglue"
	fi
else
	gecko_cv_extra_pkg_dependencies="${gecko_cv_gecko}-gtkmozembed"
fi

$1[]_EXTRA_PKG_DEPENDENCIES="$gecko_cv_extra_pkg_dependencies"
$1[]_EXTRA_LIBS="$gecko_cv_extra_libs"
$1[]_GLUE_LIBS="$gecko_cv_glue_libs"

])

# GECKO_DEFINES
#
# Defines the AM_CONDITIONALS for GECKO_INIT. This is a separate call
# so that you may call GECKO_INIT conditionally; but note that you must
# call GECKO_DEFINES _unconditionally_ !

AC_DEFUN([GECKO_DEFINES],
[
# Ensure we have an integer variable to compare with
if test -z "$gecko_cv_gecko_version_int"; then
	gecko_cv_gecko_version_int=0
fi
AM_CONDITIONAL([HAVE_MOZILLA_TOOLKIT],[test "$gecko_cv_have_gecko" = "yes" -a "$gecko_cv_gecko_flavour" = "toolkit"])
AM_CONDITIONAL([HAVE_GECKO_DEBUG],[test "$gecko_cv_have_gecko" = "yes" -a "$gecko_cv_have_debug" = "yes"])
AM_CONDITIONAL([HAVE_GECKO_1_7],[test "$gecko_cv_have_gecko" = "yes" -a "$gecko_cv_gecko_version_int" -ge "1007000"])
AM_CONDITIONAL([HAVE_GECKO_1_8],[test "$gecko_cv_have_gecko" = "yes" -a "$gecko_cv_gecko_version_int" -ge "1008000"])
AM_CONDITIONAL([HAVE_GECKO_1_8_1],[test "$gecko_cv_have_gecko" = "yes" -a "$gecko_cv_gecko_version_int" -ge "1008001"])
AM_CONDITIONAL([HAVE_GECKO_1_9],[test "$gecko_cv_have_gecko" = "yes" -a "$gecko_cv_gecko_version_int" -ge "1009000"])
AM_CONDITIONAL([HAVE_GECKO_HOME],[test "x$_GECKO_HOME" != "x"])
AM_CONDITIONAL([HAVE_GECKO_DEBUG],[test "$gecko_cv_have_debug" = "yes"])
AM_CONDITIONAL([HAVE_GECKO_XPCOM_GLUE],[test "$gecko_cv_have_xpcom_glue" = "yes"])
])

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# _GECKO_DISPATCH(MACRO, INCLUDEDIRS, ...)

m4_define([_GECKO_DISPATCH],
[

if test "$gecko_cv_have_gecko" != "yes"; then
	AC_MSG_FAILURE([Gecko not present; can't run this test!])
fi

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
_SAVE_CXXFLAGS="$CXXFLAGS"
_SAVE_LDFLAGS="$LDFLAGS"
_SAVE_LIBS="$LIBS"
if test "${gecko_cv_gecko}" = "libxul-embedding" -o "${gecko_cv_gecko}" = "libxul"; then
	CPPFLAGS="$CPPFLAGS $_GECKO_EXTRA_CPPFLAGS $_GECKO_CFLAGS $($PKG_CONFIG --cflags-only-I ${gecko_cv_gecko}-unstable)"
	CXXFLAGS="$CXXFLAGS $_GECKO_EXTRA_CXXFLAGS $_GECKO_CFLAGS $($PKG_CONFIG --cflags-only-other ${gecko_cv_gecko}-unstable)"
	LIBS="$LIBS $($PKG_CONFIG --libs ${gecko_cv_gecko}) -ldl"
else
	CPPFLAGS="$CPPFLAGS $_GECKO_EXTRA_CPPFLAGS $_GECKO_CFLAGS $($PKG_CONFIG --cflags-only-I ${gecko_cv_gecko}-xpcom)"
	CXXFLAGS="$CXXFLAGS $_GECKO_EXTRA_CXXFLAGS $_GECKO_CFLAGS $($PKG_CONFIG --cflags-only-other ${gecko_cv_gecko}-xpcom)"
	LIBS="$LIBS $($PKG_CONFIG --libs ${gecko_cv_gecko}-xpcom)"
fi
if test -n "$_GECKO_HOME"; then
	LDFLAGS="$LDFLAGS $_GECKO_EXTRA_LDFLAGS -Wl,--rpath=$_GECKO_HOME"
else
	LDFLAGS="$LDFLAGS $_GECKO_EXTRA_LDFLAGS"
fi

_GECKO_DISPATCH_INCLUDEDIRS="$2"

# Sigh Gentoo has a rubbish header layout
# http://bugs.gentoo.org/show_bug.cgi?id=100804
# Mind you, it's useful to be able to test against uninstalled mozilla builds...
_GECKO_DISPATCH_INCLUDEDIRS="$_GECKO_DISPATCH_INCLUDEDIRS dom necko pref"

# Now add them to CPPFLAGS - asac: well ... not anymore since 1.9 -> test whether they exist before adding.
for i in $_GECKO_DISPATCH_INCLUDEDIRS; do
	if test -d "$_GECKO_INCLUDE_ROOT/$i"; then
		CPPFLAGS="$CPPFLAGS -I$_GECKO_INCLUDE_ROOT/$i"
	fi
done

m4_indir([$1],m4_shiftn(2,$@))

CPPFLAGS="$_SAVE_CPPFLAGS"
CXXFLAGS="$_SAVE_CXXFLAGS"
LDFLAGS="$_SAVE_LDFLAGS"
LIBS="$_SAVE_LIBS"

AC_LANG_POP([C++])

])# _GECKO_DISPATCH

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# GECKO_CHECK_HEADERS(INCLUDEDIRS, HEADERS, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND], [INCLUDES])

AC_DEFUN([GECKO_CHECK_HEADERS],[_GECKO_DISPATCH([AC_CHECK_HEADERS],$@)])

# GECKO_COMPILE_IFELSE(INCLUDEDIRS, PROGRAM, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])

AC_DEFUN([GECKO_COMPILE_IFELSE],[_GECKO_DISPATCH([AC_COMPILE_IFELSE],$@)])

# GECKO_RUN_IFELSE(INCLUDEDIRS, PROGRAM, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])

AC_DEFUN([GECKO_RUN_IFELSE],[_GECKO_DISPATCH([AC_RUN_IFELSE],$@)])

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# GECKO_XPCOM_PROGRAM([PROLOGUE], [BODY])
#
# Produce a template C++ program which starts XPCOM up and shuts it down after
# the BODY part has run. In BODY, the the following variables are predeclared:
#
# nsresult rv
# int status = 1 (EXIT_FAILURE)
#
# The program's exit status will be |status|; set it to 0 (or EXIT_SUCCESS)
# to indicate success and to a value between 1 (EXIT_FAILURE) and 120 to
# indicate failure.
#
# To jump out of the BODY and exit the test program, you can use |break|.

AC_DEFUN([GECKO_XPCOM_PROGRAM],
[AC_LANG_PROGRAM([[
#include <mozilla-config.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef XPCOM_GLUE
#include <nsXPCOMGlue.h>
#else
#include <nsXPCOM.h>
#endif // XPCOM_GLUE

#include <nsCOMPtr.h>
#include <nsILocalFile.h>
#include <nsIServiceManager.h>
#if defined(HAVE_GECKO_1_8) || defined(HAVE_GECKO_1_9)
#include <nsStringAPI.h>
#else
#include <nsString.h>
#endif
]]
[$1],
[[

nsresult rv;
#ifdef XPCOM_GLUE
    static const GREVersionRange greVersion = {
    "1.8", PR_TRUE,
    "1.9.*", PR_TRUE
    };
    char xpcomLocation[4096];
    rv = GRE_GetGREPathWithProperties(&greVersion, 1, nsnull, 0, xpcomLocation, 4096);
    if (NS_FAILED(rv)) {
        exit(123);
    }

    // Startup the XPCOM Glue that links us up with XPCOM.
    XPCOMGlueStartup(xpcomLocation);
    if (NS_FAILED(rv)) {
        exit(124);
    }
#endif // XPCOM_GLUE

// redirect unwanted mozilla debug output to the bit bucket
freopen ("/dev/null", "w", stdout);

nsCOMPtr<nsILocalFile> directory = nsnull;
#ifndef XPCOM_GLUE
rv = NS_NewNativeLocalFile (NS_LITERAL_CSTRING("$_GECKO_HOME"), PR_FALSE,
			    getter_AddRefs (directory));
if (NS_FAILED (rv) || !directory) {
	exit (126);
}
#endif

rv = NS_InitXPCOM2 (nsnull, directory, nsnull);
if (NS_FAILED (rv)) {
	exit (125);
}

int status = EXIT_FAILURE;

// now put in the BODY, scoped with do...while(0) to ensure we don't hold a
// COMptr after XPCOM shutdown and so we can jump out with a simple |break|.
do {
]]
m4_shiftn(1,$@)
[[
} while (0);
	
NS_ShutdownXPCOM (nsnull);
exit (status);
]])
]) # GECKO_XPCOM_PROGRAM

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# GECKO_XPCOM_PROGRAM_CHECK([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND], [ACTION-IF-CROSS-COMPILING])
#
# Checks whether we can build and run any XPCOM test programs at all

AC_DEFUN([GECKO_XPCOM_PROGRAM_CHECK],
[AC_REQUIRE([GECKO_INIT])dnl

AC_CACHE_CHECK([whether we can compile and run XPCOM programs],
[gecko_cv_xpcom_program_check],
[
gecko_cv_xpcom_program_check=no

GECKO_RUN_IFELSE([],
	[GECKO_XPCOM_PROGRAM([],[[status = EXIT_SUCCESS;]])],
	[gecko_cv_xpcom_program_check=yes],
	[gecko_cv_xpcom_program_check=no],
	[gecko_cv_xpcom_program_check=maybe])
])

if test "$gecko_cv_xpcom_program_check" = "yes"; then
	ifelse([$2],,[:],[$2])
else
	ifelse([$3],,[AC_MSG_FAILURE([Cannot compile and run XPCOM programs])],
	[$3])
fi

]) # GECKO_XPCOM_PROGRAM_CHECK

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# GECKO_CHECK_CONTRACTID(CONTRACTID, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
#
# Checks wheter CONTRACTID is a registered contract ID

AC_DEFUN([GECKO_CHECK_CONTRACTID],
[AC_REQUIRE([GECKO_INIT])dnl

AS_VAR_PUSHDEF([gecko_cv_have_CID],[gecko_cv_have_$1])

AC_CACHE_CHECK([for the $1 XPCOM component],
gecko_cv_have_CID,
[
AS_VAR_SET(gecko_cv_have_CID,[no])

GECKO_RUN_IFELSE([],
[GECKO_XPCOM_PROGRAM([[
#include <nsIComponentRegistrar.h>
]],[[
status = 99;
nsCOMPtr<nsIComponentRegistrar> registrar;
rv = NS_GetComponentRegistrar (getter_AddRefs (registrar));
if (NS_FAILED (rv)) break;

status = 98;
PRBool isRegistered = PR_FALSE;
rv = registrar->IsContractIDRegistered ("$1", &isRegistered);
if (NS_FAILED (rv)) break;

status = isRegistered ? EXIT_SUCCESS : 97;
]])
],
[AS_VAR_SET(gecko_cv_have_CID,[yes])],
[AS_VAR_SET(gecko_cv_have_CID,[no])],
[AS_VAR_SET(gecko_cv_have_CID,[maybe])])

])

if test AS_VAR_GET(gecko_cv_have_CID) = "yes"; then
	ifelse([$2],,[:],[$2])
else
	ifelse([$3],,[AC_MSG_ERROR([dnl
Contract ID "$1" is not registered, but $PACKAGE_NAME depends on it.])],
	[$3])
fi

AS_VAR_POPDEF([gecko_cv_have_CID])

]) # GECKO_CHECK_CONTRACTID

# GECKO_CHECK_CONTRACTIDS(CONTRACTID, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
#
# Checks wheter CONTRACTIDs are registered contract IDs.
# If ACTION-IF-NOT-FOUND is given, it is executed when one of the contract IDs
# is not found and the missing contract ID is in the |as_contractid| variable.

AC_DEFUN([GECKO_CHECK_CONTRACTIDS],
[AC_REQUIRE([GECKO_INIT])dnl

result=yes
as_contractid=
for as_contractid in $1
do
	GECKO_CHECK_CONTRACTID([$as_contractid],[],[result=no; break;])
done

if test "$result" = "yes"; then
	ifelse([$2],,[:],[$2])
else
	ifelse([$3],,[AC_MSG_ERROR([dnl
Contract ID "$as_contractid" is not registered, but $PACKAGE_NAME depends on it.])],
	[$3])
fi

]) # GECKO_CHECK_CONTRACTIDS

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# GECKO_XPIDL([ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
#
# Checks for xpidl program and include directory
#
# Variables set:
# XPIDL:        the xpidl program
# XPIDL_IDLDIR: the xpidl include directory

AC_DEFUN([GECKO_XPIDL],
[AC_REQUIRE([GECKO_INIT])dnl

if test ${gecko_cv_gecko} = "libxul-embedding" -o ${gecko_cv_gecko} = "libxul"; then
	_GECKO_LIBDIR="`$PKG_CONFIG pkg-config --variable=sdkdir ${gecko_cv_gecko}`/bin"
else
	_GECKO_LIBDIR="`$PKG_CONFIG --variable=libdir ${gecko_cv_gecko}-xpcom`"
fi

AC_PATH_PROG([XPIDL],[xpidl],[no],[$_GECKO_LIBDIR:$PATH])

if test ${gecko_cv_gecko} = "libxul-embedding" -o ${gecko_cv_gecko} = "libxul"; then
XPIDL_IDLDIR="`$PKG_CONFIG --variable=idldir ${gecko_cv_gecko}`"
else
XPIDL_IDLDIR="`$PKG_CONFIG --variable=idldir ${gecko_cv_gecko}-xpcom`"
if test -z "$XPIDL_IDLDIR" -o ! -f "$XPIDL_IDLDIR/nsISupports.idl"; then
	XPIDL_IDLDIR="`echo $_GECKO_LIBDIR | sed -e s!lib!share/idl!`"
fi
# Some distributions (Gentoo) have it in unusual places
if test -z "$XPIDL_IDLDIR" -o ! -f "$XPIDL_IDLDIR/nsISupports.idl"; then
	XPIDL_IDLDIR="$_GECKO_INCLUDE_ROOT/idl"
fi

if test "$XPIDL" != "no" -a -n "$XPIDL_IDLDIR" -a -f "$XPIDL_IDLDIR/nsISupports.idl"; then
	ifelse([$1],,[:],[$1])
else
	ifelse([$2],,[AC_MSG_FAILURE([XPIDL program or include directory not found])],[$2])
fi

])
