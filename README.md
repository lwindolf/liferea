![screenshot](https://lzone.de/liferea/screenshots/screenshot2.png)

Liferea is a desktop feed reader/news aggregator that brings together all of the content from your favorite subscriptions into a simple interface that makes it easy to organize and browse feeds. Its GUI is similar to a desktop mail/news client, with an embedded web browser.

[![Build Status](https://github.com/lwindolf/liferea/actions/workflows/cb.yml/badge.svg)](https://github.com/lwindolf/liferea/actions/workflows/cb.yml)

## Installation from Package

Many distributions have packaged Liferea:

[![Packages](https://repology.org/badge/latest-versions/liferea.svg)](https://repology.org/metapackage/liferea/versions)
[![Packages](https://repology.org/badge/tiny-repos/liferea.svg)](https://repology.org/metapackage/liferea/versions)


## Building

Compile with

    meson setup builddir
    meson compile -C builddir
    meson test    -C builddir

To install

    meson install -C builddir

If you compile with a --prefix directory which does not match $XDG_DATA_DIRS
you will get a runtime error about the schema not being found. To workaround
set $XDG_DATA_DIRS before starting Liferea. For example:

    env XDG_DATA_DIRS="$my_dir/share:$XDG_DATA_DIRS" $my_dir/bin/liferea

## Update JS Dependencies

    npm i
    npm audit fix
    npm run installDeps
