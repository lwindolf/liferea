%define dfi %(which desktop-file-install &>/dev/null; echo $?)

Summary: Liferea (Linux Feed Reader)
Name: liferea
Version: 0.3.9
Release: 1
Group: Productivity/Networking/Web/Browsers
Copyright: GPL
Source: liferea-%{version}.tar.gz
URL: http://liferea.sourceforge.net
Packager: Lars Lindner <lars.lindner@gmx.net>
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
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/local
make install-strip prefix=$RPM_BUILD_ROOT/usr/local sysconfdir=$RPM_BUILD_ROOT/etc
%if %{dfi}
%else
	desktop-file-install --vendor net                  \
		--dir $RPM_BUILD_ROOT/usr/local/share/applications \
		$RPM_BUILD_ROOT/usr/local/share/applications/net-liferea.desktop
%endif

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
/usr/local/share/pixmaps/liferea.xpm
