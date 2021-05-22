# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2009-2014, HGST, Inc. All rights reserved.
# Copyright (c) 2020 Western Digital Corporation or its affiliates.

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lib tools
if BUILD_TEST
SUBDIRS += test/programs
endif

EXTRA_DIST = autogen.sh COPYING.BSD COPYING.LESSER README.md

if ENABLE_RPMBUILD
rpmdir = $(abs_top_builddir)/rpmbuild

EXTRA_DIST += libzbc.spec
RPMARCH=`$(RPM) --eval %_target_cpu`

rpm: dist
	@echo "Building RPM packages..."
	@mkdir -p $(rpmdir)
	@mv libzbc-$(PACKAGE_VERSION).tar.gz v$(PACKAGE_VERSION).tar.gz
	$(RPMBUILD) -ta --clean -D "_topdir $(rpmdir)" v$(PACKAGE_VERSION).tar.gz
	@mv -f $(rpmdir)/RPMS/$(RPMARCH)/*.rpm $(abs_top_builddir)
	@mv -f $(rpmdir)/SRPMS/*.rpm $(abs_top_builddir)
	@rm -rf $(rpmdir)
	@rm -f v$(PACKAGE_VERSION).tar.gz
else
rpm:
	@echo "To build RPM packages, install the rpm and rpm-build packages"
	exit 1
endif
