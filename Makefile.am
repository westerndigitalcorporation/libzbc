# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = . $(subdirs)
EXTRA_DIST = \
	autogen.sh \
	exports

AM_CPPFLAGS = \
	$(CFLAGS) \
	-Wall -Wextra -Wno-unused-parameter \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include

pkgconfdir = $(libdir)/pkgconfig
rpmdir = $(abs_top_builddir)/rpmbuild

pkgconf_DATA = libzbc.pc
pkginclude_HEADERS = include/libzbc/zbc.h

noinst_HEADERS = include/zbc_private.h
bin_PROGRAMS =
lib_LTLIBRARIES =
noinst_PROGRAMS =

libzbc_ldadd = $(LDADD) libzbc.la

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

if BUILD_GUI
include tools/gzbc/Makemodule.am
include tools/gzviewer/Makemodule.am
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
endif

if ENABLE_RPMBUILD
EXTRA_DIST += libzbc.spec
RPMARCH=`$(RPM) --eval %_target_cpu`

spec: libzbc.spec.in
	@echo "Generating rpm spec file..."
	@echo "Version:        $(PACKAGE_VERSION)" > libzbc.spec
	@cat libzbc.spec.in | grep -v '# ' >> libzbc.spec

rpm: spec dist
	@echo "Building RPM packages..."
	@mkdir -p $(rpmdir)
	$(RPMBUILD) -ta --clean -D "_topdir $(rpmdir)" libzbc-$(PACKAGE_VERSION).tar.gz
	@mv -f $(rpmdir)/RPMS/$(RPMARCH)/*.rpm $(abs_top_builddir)
	@mv -f $(rpmdir)/SRPMS/*.rpm $(abs_top_builddir)
	@rm -rf $(rpmdir)
	@rm -f libzbc-$(PACKAGE_VERSION).tar.gz
	@rm -f libzbc.spec
else
rpm:
	@echo "To build RPM packages, install the rpm and rpm-build packages"
	exit 1
endif
