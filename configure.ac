
AC_INIT([libzbc], [2.0.0], [damien.lemoal@hgst.com, adam.manzanares@hgst.com])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([include/config.h])
AC_PREFIX_DEFAULT(/usr)

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O

AC_SYS_LARGEFILE

AM_PROG_AR
LT_INIT

ACX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

PKG_CHECK_MODULES([GTK], [gtk+-3.0])

AC_CONFIG_FILES([
	libzbc.pc
	Makefile
])

# Build lkvs 
#AC_CONFIG_SUBDIRS([tools/lkvs])

AC_OUTPUT

