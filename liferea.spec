Summary: Liferea (Linux RSS News Aggregator)
Name: liferea
Version: 0.4.3
Release: 0
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
/usr/local/share/liferea/pixmaps/flag.png
/usr/local/share/liferea/pixmaps/unavailable.png
/usr/local/share/liferea/pixmaps/available.png
/usr/local/share/liferea/pixmaps/directory.png
/usr/local/share/liferea/pixmaps/ocs.png
/usr/local/share/liferea/pixmaps/help.png
/usr/local/share/liferea/pixmaps/vfolder.png
/usr/local/share/liferea/pixmaps/empty.png
