Summary: Liferea (Linux RSS News Aggregator)
Name: liferea
Version: 0.4.8
Release: 1
Group: Applications/Internet
Copyright: GPL
Source: liferea-%{version}.tar.gz
URL: http://liferea.sourceforge.net/
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: gtk2-devel
BuildRequires: libxml2-devel >= 2.5.10
BuildRequires: GConf2-devel
BuildRequires: gettext
Requires: gtk2 libxml2 gconf2

%description
Liferea (Linux Feed Reader) is an RSS/RDF news 
aggregator which also supports CDF channels, 
Atom/Echo/PIE feeds and OCS directories. It 
is intended to be a clone of the Windows-only 
FeedReader.

%prep
%setup -q -n %{name}-%{version}

%build
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--with%{!?debug:out}-debug
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr
make install-strip prefix=$RPM_BUILD_ROOT/usr sysconfdir=$RPM_BUILD_ROOT/etc
%find_lang %{name}

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}.lang
%defattr(-,root,root)
%doc COPYING AUTHORS NEWS README ChangeLog
%{_bindir}/liferea
%{_bindir}/liferea-bin
%{_datadir}/liferea
%{_datadir}/applications/liferea.desktop
%{_libdir}/*.so.*
%exclude %{_libdir}/*.la

%changelog

* Fri Apr  2 2004 Brian Pepple <bdpepple@ameritech.net> 0.4.8-1
- Added BuildRequires.
- Added gettext support.
- Added macros for files.
