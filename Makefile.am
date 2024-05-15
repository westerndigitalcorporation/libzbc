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

EXTRA_DIST = autogen.sh \
	     README.md \
	     LICENSES/BSD-2-Clause.txt \
	     LICENSES/LGPL-3.0-or-later.txt

if BUILDING_RPM
rpmdir = $(abs_top_builddir)/rpmbuild

EXTRA_DIST += libzbc.spec
RPMARCH=`$(RPM) --eval %_target_cpu`

rpm: dist
	@echo "Building RPM packages..."
	@mkdir -p $(rpmdir)
	$(RPMBUILD) -ta --clean \
                -D "_topdir $(rpmdir)" \
                -D "pkg_name $(PACKAGE_NAME)" \
                -D "pkg_version $(PACKAGE_VERSION)" \
		libzbc-$(PACKAGE_VERSION).tar.gz
	@mv -f $(rpmdir)/RPMS/$(RPMARCH)/*.rpm $(abs_top_builddir)
	@mv -f $(rpmdir)/SRPMS/*.rpm $(abs_top_builddir)
	@rm -rf $(rpmdir)
	@rm -f libzbc-$(PACKAGE_VERSION).tar.gz
else
rpm:
	@echo "Building RPM packages requires rpmbuild and rpm utilities"
	exit 1
endif
