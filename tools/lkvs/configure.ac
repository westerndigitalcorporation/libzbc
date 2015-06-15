
AC_INIT(lkvs, 1.0, adam.manzanares@hgst.com)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(m4)
AM_INIT_AUTOMAKE
LT_INIT
AC_ENABLE_SHARED
AC_DISABLE_STATIC

# Conditionals

# Build python bindings
AC_ARG_WITH([pybind],
            [AS_HELP_STRING([--with-pybind], [build python bindings])],
			[case "${withval}" in
				yes) with_pybind=yes ;;
				no)  with_pybind=no ;;
				*) AC_MSG_ERROR([bad value ${withval} for --with-pybind]) ;;
				esac],
			[with_pybind=no])
AM_CONDITIONAL(WITH_PYBIND, test "$with_pybind" = "yes")

# Build java bindings
AC_ARG_WITH([javabind],
            [AS_HELP_STRING([--with-javabind], [build java bindings])],
			[case "${withval}" in
				yes) with_javabind=yes ;;
				no) with_javabind=no ;;
				*) AC_MSG_ERROR([bad value ${withval} for --with-javabind]) ;;
				esac],
			[with_javabind=no])
AM_CONDITIONAL(WITH_JAVABIND, test "$with_javabind" = "yes")

# Start checking 
AC_PROG_CXX

# We need to build gtest 
AC_CONFIG_SUBDIRS([gtest])

# Generate these Makefiles
AC_CONFIG_FILES([Makefile 
	src/Makefile 
	src/liblkvs/Makefile 
	src/examples/Makefile
	src/test/Makefile
	src/pybind/Makefile
	src/javabind/Makefile])

AC_OUTPUT
