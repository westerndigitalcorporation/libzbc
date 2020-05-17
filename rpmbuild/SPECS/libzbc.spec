Name:           libzbc
Version:        %version
Release:        1%{?dist}
License:        SPDX-License-Identifier: BSD-2-Clause
Summary:        A library to control zoned SCSI/ATA devices
Source:         %archive

BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  gcc

%description
libzbc is a simple library providing functions for manipulating SCSI and ATA
devices supporting the Zoned Block Command (ZBC) and Zoned-device ATA command
set (ZAC) specifications.
libzbc implementation is compliant with the latest drafts of the ZBC and ZAC
standards defined by INCITS technical committee T10 and T13 (respectively).

%package devel
Summary: Development header files for libzbc
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
This package provides development header files for libzbc.

%prep
%setup
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT

%build
sh autogen.sh
%configure --libdir="%{_libdir}" --includedir="%{_includedir}"
make

%install
make install PREFIX=%{_prefix} DESTDIR=$RPM_BUILD_ROOT

find $RPM_BUILD_ROOT -name '*.la' -delete

%ldconfig_scriptlets

%files
%{_bindir}/*
%{_libdir}/*

%files devel
%{_includedir}/*

%clean
mv -f %{_topdir}/RPMS/%{?_isa}/*.rpm %{_topdir}/../
rm -rf %{_topdir}/BUILDROOT
rm -rf %{_topdir}/BUILD/*
rm -rf %{_topdir}/RPMS/*
rm -rf %{_topdir}/SOURCES/*
rm -rf %{_topdir}/SRPMS/*
