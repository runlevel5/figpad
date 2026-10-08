%global app_id  com.github.runlevel5.Figpad

Name:           figpad
Version:        0.1.0
Release:        3%{?dist}
Summary:        Simple GTK 4 text editor

License:        GPL-2.0-or-later
URL:            https://github.com/runlevel5/figpad
# Generate from a checkout with:
#   git archive --prefix=figpad-0.1.0/ -o figpad-0.1.0.tar.gz HEAD
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  meson
BuildRequires:  gettext
BuildRequires:  desktop-file-utils
BuildRequires:  pkgconfig(gtk4) >= 4.18.0

Requires:       hicolor-icon-theme

%description
Figpad is a simple GTK 4 text editor that emphasizes simplicity. Only
the most essential features are implemented, so it is easy to use,
requires few libraries and starts up quickly. It is based on l3afpad,
a GTK 3 port of Leafpad.

%prep
%autosetup

%build
%meson
%meson_build

%install
%meson_install
install -Dpm 0644 figpad.1 %{buildroot}%{_mandir}/man1/figpad.1
%find_lang %{name}

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{app_id}.desktop

%files -f %{name}.lang
%license COPYING
%doc AUTHORS ChangeLog NEWS README.md
%{_bindir}/figpad
%{_datadir}/applications/%{app_id}.desktop
%{_datadir}/icons/hicolor/*/apps/%{app_id}.png
%{_datadir}/icons/hicolor/scalable/apps/%{app_id}.svg
%{_datadir}/pixmaps/figpad.png
%{_datadir}/pixmaps/figpad.xpm
%{_mandir}/man1/figpad.1*

%changelog
* Thu Oct 08 2026 Trung Lê <trung.le@ruby-journal.com> - 0.1.0-3
- Credit Jakub Steiner and gnoman for the app icon

* Thu Oct 08 2026 Trung Lê <trung.le@ruby-journal.com> - 0.1.0-2
- Use com.github.runlevel5.Figpad as app ID, desktop file and icon name

* Thu Oct 08 2026 Trung Lê <trung.le@ruby-journal.com> - 0.1.0-1
- Initial package
