Summary: Liferea (Linux Feed Reader)
Name: liferea
Version: 0.3.5
Release: 1
Group: Productivity/Networking/Web/Browsers
Copyright: GPL
Source: liferea-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root
Requires: gtk2 libxml2 libgtkhtml

%description
Liferea (Linux Feed Reader) is an RSS/RDF feed reader. 
It's intended to be a clone of the Windows-only FeedReader. 
It can be used to maintain a list of subscribed feeds, 
browse through their items, and show their contents 
using GtkHTML.

%prep
%setup

%build
./configure \
	--with%{!?debug:out}-debug
patch -p0 -N < /home/lars/tech/coding/c/liferea/wrong-size.patch || echo patch ignored
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
