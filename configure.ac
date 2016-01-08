
AC_INIT([libzbc], [4.0.0], [damien.lemoal@hgst.com, adam.manzanares@hgst.com])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([include/config.h])
AC_PREFIX_DEFAULT(/usr)

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O

AC_SYS_LARGEFILE

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
+m4_pattern_allow([AM_PROG_AR])
LT_INIT

ACX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

# Conditionals

# Build gzbc only if GTK3 is installed.
PKG_CHECK_MODULES([GTK], [gtk+-3.0], [HAVE_GTK3=1], [HAVE_GTK3=0])
AM_CONDITIONAL([BUILD_GZBC], [test "$HAVE_GTK3" -eq 1])

# Build test suite
AC_ARG_WITH([test],
            [AS_HELP_STRING([--with-test], [Build compatibility test suite [default=no]])],
            [WITH_TEST=1],
            [WITH_TEST=0])
AM_CONDITIONAL([BUILD_TEST], [test "$WITH_TEST" -eq 1])

AC_CONFIG_FILES([
	libzbc.pc
	Makefile
])

# Build lkvs 
#AC_CONFIG_SUBDIRS([tools/lkvs])

AC_OUTPUT

