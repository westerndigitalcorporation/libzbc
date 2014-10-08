
ACLOCAL_AMFLAGS = -I m4
SUBDIRS = . $(subdirs)
EXTRA_DIST = \
	autogen.sh \
	exports

AM_CPPFLAGS = \
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
include tools/read_zone/Makemodule.am
include tools/report_zones/Makemodule.am
include tools/reset_write_ptr/Makemodule.am
include tools/set_write_ptr/Makemodule.am
include tools/set_zones/Makemodule.am
include tools/write_zone/Makemodule.am
include tools/gui/Makemodule.am

