
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = . $(subdirs)
EXTRA_DIST = \
	autogen.sh \
	exports

AM_CPPFLAGS = \
	-O2 \
	-Wall -Wextra -Wno-unused-parameter \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include

pkgconfdir = $(libdir)/pkgconfig

pkgconf_DATA = libzbc.pc
pkginclude_HEADERS =
noinst_HEADERS =
bin_PROGRAMS =
lib_LTLIBRARIES =
noinst_PROGRAMS =
dist_bin_SCRIPTS =

libzbc_ldadd = $(LDADD) libzbc.la

include include/Makemodule.am
include lib/Makemodule.am
include tools/zone_op/Makemodule.am

include tools/info/Makemodule.am
include tools/report_zones/Makemodule.am
include tools/reset_zone/Makemodule.am
include tools/open_zone/Makemodule.am
include tools/close_zone/Makemodule.am
include tools/finish_zone/Makemodule.am
include tools/read_zone/Makemodule.am
include tools/write_zone/Makemodule.am

include tools/set_write_ptr/Makemodule.am
include tools/set_zones/Makemodule.am

include tools/report_realms/Makemodule.am
include tools/convert_realms/Makemodule.am
include tools/cmr_wp_check/Makemodule.am

if BUILD_GZBC
include tools/gui/Makemodule.am
endif

if BUILD_TEST
include test/programs/print_devinfo/Makemodule.am
include test/programs/report_zones/Makemodule.am
include test/programs/reset_zone/Makemodule.am
include test/programs/open_zone/Makemodule.am
include test/programs/close_zone/Makemodule.am
include test/programs/finish_zone/Makemodule.am
include test/programs/read_zone/Makemodule.am
include test/programs/write_zone/Makemodule.am
include test/programs/report_realms/Makemodule.am
include test/programs/convert_realms/Makemodule.am
endif

