Name:           mousedamper
Version:        0.9.1
Release:        1%{?dist}
Summary:        Mouse damper to prevent accidental clicks

License:        GPL-3.0-or-later
URL:            https://github.com/miketwebster/mouse-damper
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  meson >= 0.56.0
BuildRequires:  ninja-build
BuildRequires:  gcc
BuildRequires:  glib2-devel >= 2.50
BuildRequires:  libevdev-devel
BuildRequires:  pkgconfig
BuildRequires:  python3
BuildRequires:  gettext

%description
Mouse Damper is a utility that helps prevent accidental clicks caused by
hand tremors or unsteady mouse movements by temporarily freezing the mouse
pointer during button presses.

%package common
Summary:        Common files for Mouse Damper
Requires:       python3
Requires:       python3-gobject
Requires:       gtk3
Requires:       xapps
BuildArch:      noarch

%description common
This package contains scripts, icons and data files for mousedamper.

%prep
%autosetup

%build
%meson --buildtype=debugoptimized
%meson_build

%install
%meson_install

# Set setuid bit on daemon (like debian does)
chmod 4755 %{buildroot}%{_libexecdir}/mousedamper/mousedamper

%files
%license COPYING
%{_libexecdir}/mousedamper/mousedamper
%{_datadir}/glib-2.0/schemas/org.mtw.mousedamper.gschema.xml

%files common
%{_bindir}/mousedamper-config
%{_bindir}/mousedamper-launch
%{_libexecdir}/mousedamper/config.py
%{_libexecdir}/mousedamper/mousedamper-config.py
%{_libexecdir}/mousedamper/mousedamper-launch.py
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/*/apps/mousedamper*.svg

%post
/usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &>/dev/null || :

%postun
/usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &>/dev/null || :

%post common
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

%postun common
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

%changelog
* Sat Feb 15 2026 Michael Webster <miketwebster@gmail.com> - 0.9.1-1
- Bundle required MinGW runtime DLLs in Windows installer (fixes #2)

* Tue Jan 06 2026 Michael Webster <miketwebster@gmail.com> - 0.9.0-1
- Initial RPM packaging
