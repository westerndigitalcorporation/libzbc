
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

libzbc_ldadd = $(LDADD) libzbc.la

include include/Makemodule.am
include lib/Makemodule.am
include tools/report_zones/Makemodule.am
include tools/reset_write_ptr/Makemodule.am
include tools/read_zone/Makemodule.am
include tools/write_zone/Makemodule.am
include tools/open_zone/Makemodule.am
include tools/close_zone/Makemodule.am
include tools/finish_zone/Makemodule.am
include tools/set_write_ptr/Makemodule.am
include tools/set_zones/Makemodule.am

if BUILD_GZBC
include tools/gui/Makemodule.am
endif

if BUILD_TESTS
include test/print_devinfo/Makemodule.am
include test/report_zones/Makemodule.am
include test/reset_write_ptr/Makemodule.am
include test/open_zone/Makemodule.am
include test/close_zone/Makemodule.am
include test/finish_zone/Makemodule.am
include test/read_zone/Makemodule.am
include test/write_zone/Makemodule.am
endif

