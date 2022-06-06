Name:		libzbc
Version:	5.12.0
Release:	1%{?dist}
Summary:	A library to control SCSI/ZBC and ATA/ZAC devices

License:	BSD and LGPLv3+
URL:		https://github.com/westerndigitalcorporation/%{name}
Source0:	%{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:	gtk3-devel
BuildRequires:	autoconf
BuildRequires:	automake
BuildRequires:	libtool
BuildRequires:	make
BuildRequires:	gcc

%description
libzbc is a SCSI and ATA passthrough command library providing functions for
managing SCSI and ATA devices supporting the Zoned Block Command (ZBC) and
Zoned-device ATA command set (ZAC) specifications. libzbc implementation is
compliant with the ZBC and ZAC r05 standards defined by INCITS technical
committee T10 and T13 (respectively).

# Development headers package
%package devel
Summary: Development header files for libzbc
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
This package provides development header files for libzbc.

# Command line tools package
%package cli-tools
Summary: Command line tools using libzbc
Requires: %{name}%{?_isa} = %{version}-%{release}

%description cli-tools
This package provides command line tools using libzbc.

# Graphic tools package
%package gtk-tools
Summary: GTK tools using libzbc
Requires: %{name}%{?_isa} = %{version}-%{release}

%description gtk-tools
This package provides GTK-based graphical tools using libzbc.

%prep
%autosetup

%build
sh autogen.sh
%configure --libdir="%{_libdir}" --includedir="%{_includedir}"
%make_build

%install
%make_install PREFIX=%{_prefix}
chmod -x ${RPM_BUILD_ROOT}%{_mandir}/man8/*.8*

find ${RPM_BUILD_ROOT} -name '*.la' -delete

%ldconfig_scriptlets

%files
%{_libdir}/*.so.*
%exclude %{_libdir}/*.a
%exclude %{_libdir}/pkgconfig/*.pc
%license LICENSES/BSD-2-Clause.txt
%license LICENSES/LGPL-3.0-or-later.txt
%doc README.md

%files devel
%{_includedir}/*
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc
%license LICENSES/BSD-2-Clause.txt
%license LICENSES/LGPL-3.0-or-later.txt

%files cli-tools
%{_bindir}/zbc_*
%{_mandir}/man8/zbc_*.8*
%license LICENSES/LGPL-3.0-or-later.txt

%files gtk-tools
%{_bindir}/gzbc
%{_bindir}/gzviewer
%{_mandir}/man8/gzbc.8*
%{_mandir}/man8/gzviewer.8*
%license LICENSES/LGPL-3.0-or-later.txt

%changelog
* Tue Dec 07 2021 Damien Le Moal <damien.lemoal@wdc.com> 5.12.0-1
- Version 5.12.0 package
