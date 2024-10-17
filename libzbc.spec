Name:           %{pkg_name}
Version:        %{pkg_version}
Release:        1%{?dist}
Summary:	A library to control SCSI/ZBC and ATA/ZAC devices

License:	BSD and LGPLv3+
URL:		https://github.com/westerndigitalcorporation/%{name}
Source0:	%{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:	desktop-file-utils
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
compliant with the ZBC-2 and ZAC-2 standards defined by INCITS technical
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

desktop-file-validate %{buildroot}/%{_datadir}/applications/gzbc.desktop
desktop-file-validate %{buildroot}/%{_datadir}/applications/gzviewer.desktop

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
%{_datadir}/polkit-1/actions/org.gnome.gzbc.policy
%{_datadir}/applications/gzbc.desktop
%{_datadir}/pixmaps/gzbc.png
%{_bindir}/gzviewer
%{_datadir}/polkit-1/actions/org.gnome.gzviewer.policy
%{_datadir}/applications/gzviewer.desktop
%{_datadir}/pixmaps/gzviewer.png
%{_mandir}/man8/gzbc.8*
%{_mandir}/man8/gzviewer.8*
%license LICENSES/LGPL-3.0-or-later.txt

%changelog
* Wed Oct 16 2024 Dmitry Fomichev <dmitry.fomichev@wdc.com> 6.2.0-1
- Version 6.2.0 package
