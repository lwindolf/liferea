Summary: Liferea (Linux RSS News Aggregator)
Name: liferea
Version: 0.4.8
Release: 1
Group: Applications/Internet
Copyright: GPL
Source: liferea-%{version}.tar.gz
URL: http://liferea.sourceforge.net/
BuildRoot: %{_tmppath}/%{name}-root
Requires: gtk2 libxml2 gconf2

%description
Liferea (Linux Feed Reader) is an RSS/RDF news 
aggregator which also supports CDF channels, 
Atom/Echo/PIE feeds and OCS directories. It 
is intended to be a clone of the Windows-only 
FeedReader.

%prep
%setup

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

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc COPYING AUTHORS NEWS README ChangeLog
/usr/bin/liferea
/usr/bin/liferea-bin
/usr/share/liferea
/usr/share/locale
/usr/share/applications/liferea.desktop
/usr/lib/liferea/liblihtmlg.la
/usr/lib/liferea/liblihtmlg.so.0.0.0
/usr/lib/liferea/liblihtmlm.la
/usr/lib/liferea/liblihtmlm.so.0.0.0
/usr/lib/liferea/liblihtmlt.la
/usr/lib/liferea/liblihtmlt.so.0.0.0
