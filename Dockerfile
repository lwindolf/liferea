FROM ubuntu:18.04
MAINTAINER Lars Windolf

RUN apt-get update
RUN apt-get install -y libtool intltool gcc automake autoconf
RUN apt-get install -y libxml2-dev libxslt1-dev libgtk-3-dev libwebkit2gtk-4.0-dev libpeas-dev libsqlite3-dev libjson-glib-dev libgirepository1.0-dev gsettings-desktop-schemas-dev

RUN mkdir -p /src/
WORKDIR /src/

COPY . /src/
RUN ./autogen.sh
RUN ./configure && make && cd src/tests && make test

