Summary: Liferea (Linux Feed Reader)
Name: liferea
Version: 0.4.1
Release: 1
Group: Productivity/Networking/Web/Browsers
Copyright: GPL
Source: liferea-%{version}.tar.gz
URL: http://liferea.sourceforge.net
Packager: Lars Lindner <lars.lindner@gmx.net>
BuildRoot: %{_tmppath}/%{name}-root
Requires: gtk2 libxml2 libgtkhtml

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
	--with%{!?debug:out}-debug
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/local
make install-strip prefix=$RPM_BUILD_ROOT/usr/local sysconfdir=$RPM_BUILD_ROOT/etc

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,bin,bin)
%doc COPYING AUTHORS NEWS README ChangeLog
/usr/local/bin/liferea
/usr/local/share/liferea/pixmaps/read.xpm
/usr/local/share/liferea/pixmaps/unread.xpm
/usr/local/share/liferea/pixmaps/unavailable.xpm
/usr/local/share/liferea/pixmaps/available.xpm
/usr/local/share/liferea/pixmaps/directory.xpm
/usr/local/share/liferea/pixmaps/ocs.xpm
/usr/local/share/liferea/pixmaps/help.xpm
/usr/local/share/liferea/pixmaps/vfolder.xpm
/usr/local/share/liferea/pixmaps/empty.xpm
