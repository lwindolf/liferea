dnl
dnl LIFERE_CONFIG_NICE(filename)
dnl
dnl Generates the config.nice file
dnl
AC_DEFUN([LIFEREA_CONFIG_NICE],[
  AC_REQUIRE([AC_PROG_EGREP])
  AC_REQUIRE([LT_AC_PROG_SED])
  test -f $1 && mv $1 $1.old
  rm -f $1.old
  cat >$1<<EOF
#! /bin/sh
#
# Created by configure

echo "WARNING: reusing configure flags from the previous run"

EOF
  echo "'[$]0' \\" >> $1
  if test `expr -- [$]0 : "'.*"` = 0; then
    CONFIGURE_COMMAND="$CONFIGURE_COMMAND '[$]0'"
  else 
    CONFIGURE_COMMAND="$CONFIGURE_COMMAND [$]0"
  fi
  for arg in $ac_configure_args; do
     if test `expr -- $arg : "'.*"` = 0; then
        if test `expr -- $arg : "--.*"` = 0; then
       	  break;
        fi
        echo "'[$]arg' \\" >> $1
        CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS '[$]arg'"
     else
        if test `expr -- $arg : "'--.*"` = 0; then
       	  break;
        fi
        echo "[$]arg \\" >> $1
        CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS [$]arg"
     fi
  done
  echo '"[$]@"' >> $1
  chmod +x $1
  CONFIGURE_COMMAND="$CONFIGURE_COMMAND $CONFIGURE_OPTIONS"
])
