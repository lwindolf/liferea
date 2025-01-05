[![Build Status](https://github.com/lwindolf/liferea/actions/workflows/cb.yml/badge.svg)](https://github.com/lwindolf/liferea/actions/workflows/cb.yml)

[![Packages](https://repology.org/badge/latest-versions/liferea.svg)](https://repology.org/metapackage/liferea/versions)
[![Packages](https://repology.org/badge/tiny-repos/liferea.svg)](https://repology.org/metapackage/liferea/versions)
[![Dependency](https://img.shields.io/librariesio/github/lwindolf/liferea)](https://libraries.io/github/lwindolf/liferea)

## Introduction

Liferea is a desktop feed reader/news aggregator that brings together all of the content from your favorite subscriptions into a simple interface that makes it easy to organize and browse feeds. Its GUI is similar to a desktop mail/news client, with an embedded web browser.

![screenshot](https://lzone.de/liferea/screenshots/screenshot2.png)


## Installation from Package

For distro-specific package installation check out https://lzone.de/liferea/install.htm


## Building

Compile with

    meson setup builddir
    meson compile -C builddir

To install

    ninja install

If you compile with a --prefix directory which does not match $XDG_DATA_DIRS
you will get a runtime error about the schema not being found. To workaround
set $XDG_DATA_DIRS before starting Liferea. For example:

    env XDG_DATA_DIRS="$my_dir/share:$XDG_DATA_DIRS" $my_dir/bin/liferea


