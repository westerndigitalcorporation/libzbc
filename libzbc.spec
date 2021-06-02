# SPDX-License-Identifier: BSD-2-Clause
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2020 Western Digital Corporation or its affiliates.
Name:		libzbc
Version:	5.10.0
Release:	1%{?dist}
Summary:	A library to control SCSI/ZBC and ATA/ZAC zoned devices

License:	BSD and LGPLv3+
URL:		https://github.com/westerndigitalcorporation/%{name}
Source0:	https://github.com/westerndigitalcorporation/%{name}/archive/refs/tags/v%{version}.tar.gz

BuildRoot:	%{_topdir}/BUILDROOT/
BuildRequires:	autoconf,autoconf-archive,automake,libtool

%description
libzbc is a library providing functions for manipulating SCSI and ATA
devices supporting the Zoned Block Command (ZBC) and Zoned-device ATA command
set (ZAC) specifications.
libzbc implementation is compliant with the ZBC and ZAC v1 standards
defined by INCITS technical committee T10 and T13 (respectively).

%package static
Summary: Static library for libzbc
Requires: %{name}%{?_isa} = %{version}-%{release}

%description static
This package provides libzbc static library.

%package devel
Summary: Development header files for libzbc
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
This package provides development header files for libzbc.

%prep
%autosetup

%build
sh autogen.sh
%configure --libdir="%{_libdir}" --includedir="%{_includedir}"
%make_build

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
make install PREFIX=%{_prefix} DESTDIR=$RPM_BUILD_ROOT
chmod -x $RPM_BUILD_ROOT%{_mandir}/man8/*.8

find $RPM_BUILD_ROOT -name '*.la' -delete

%ldconfig_scriptlets

%files
%{_libdir}/*.so
%{_bindir}/*
%{_mandir}/man8/*
%exclude %{_libdir}/pkgconfig

%files static
%{_libdir}/*.a

%files devel
%{_includedir}/*
%{_libdir}/pkgconfig

%license COPYING.BSD COPYING.LESSER
%doc README.md

%changelog
* Wed Jun 2 2021 Damien Le Moal <damien.lemoal@wdc.com> 5.10.0-1
- Move static library to its own rpm
* Sat May 22 2021 Damien Le Moal <damien.lemoal@wdc.com> 5.10.0-1
- Version 5.10.0 initial package
