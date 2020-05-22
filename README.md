[![Build Status](https://travis-ci.org/lwindolf/liferea.svg?branch=master)](https://travis-ci.org/lwindolf/liferea) ![Coverity Scan Build Status](https://scan.coverity.com/projects/4287/badge.svg)


Introduction
------------

Liferea is a desktop feed reader/news aggregator that brings together all of the content from your favorite subscriptions into a simple interface that makes it easy to organize and browse feeds. Its GUI is similar to a desktop mail/news client, with an embedded web browser.

![screenshot](https://lzone.de/liferea/screenshots/screenshot3.png)



Installation from Package
-------------------------

For distro specific package installation check out https://lzone.de/liferea/install.htm



Building Liferea Yourself
------------------------

This section describes how to compile Liferea yourself. If you have
any problems compiling the source file an issue at Github and we will
help you asap.


###### _Mandatory Dependencies_

   libxml2-dev libxslt1-dev libsqlite3-dev libwebkit2gtk-4.0-dev libjson-glib-dev libgirepository1.0
   libpeas-dev gsettings-desktop-schemas-dev python3

   
###### _Compiling from Tarball_

Download a tarball from https://github.com/lwindolf/liferea/releases
and extract and compile with

    tar jxvf liferea-1.12.0.tar.bz2 
    ./configure
    make
    make install


###### _Compiling from Git_

Check out the code:

    git clone https://github.com/lwindolf/liferea.git

Then build it with:

    ./autogen.sh
    make
    make install

If you compile with a --prefix directory which does not match $XDG_DATA_DIRS
you will get a runtime error about the schema not being found. To workaround
set $XDG_DATA_DIRS before starting Liferea. For example:

    my_dir=$HOME/tmp/liferea
    ./autogen.sh --prefix=$my_dir
    make
    make install
    env XDG_DATA_DIRS="$my_dir/share:$XDG_DATA_DIRS" $my_dir/bin/liferea


Contributing
------------

As the project is hosted at Github pull requests and tickets via Github
are the best way to contribute to Liferea.


###### _Translating_

Before starting to translate you need a translation editor. We suggest
to use poedit or gtranslator. Please edit the translation using such a 
translation editor and send us the resulting file. Once you have finished
your work please send us the resulting file.

Please do not send translation patches. Those are a lot of work to merge
and the bandwidth saving is not that huge!


###### _New Translations_

To create a new translation you must load the translation template, which you
can find in the release tarball as "po/liferea.pot", into the translation 
editor. After editing it save it under a new name (usually your locales name
with the extension ".po").


###### _Updating Translations_

When updating an existing translation please ensure to respect earlier 
translators work. If the latest translation is only a few months old please
contact the latest translator first asking him to review your changes especially
if you change already translated literals.


###### _Localizing Feed Lists_

When Liferea starts for the first time it installs a localized feed list
if available. If this is not the case for your locale you might want to provide
one. To check if there is one for your country have a look into the "opml"
subdirectory in the latest release tarball or GIT.

If you want to provide/update a localized feed list please follow these rules:

+ Keep the English part of the default feed list
+ Only add neutral content feeds (no sex, no ideologic politics, no illegal stuff)
+ Provide good and short feed titles
+ Provide HTML URLs for each feed.

###### _Creating Plugins_

Liferea 1.10+ support GObject Introspection based plugins using libpeas. The
Liferea distribution comes with a set of Python plugin e.g. the media player,
libsecret support, a tray icon plugin and maybe others.


###### Why We Use Plugins?

The idea behind plugins is to extend Liferea without changing compile time
requirements. With the plugin only activating if all its bindings are available
Liferea uses plugins to automatically enable features where possible.


###### How Plugins Interact With Liferea

You can develop plugins for your private use or contribute them upstream.
In any case it makes sense to start by cloning one of the existing plugins
and to think about how to hook into Liferea. There are two common ways:

+ using interfaces,
+ or by listening to events on Liferea objects,
+ or not at all by just controlling Liferea from the outside.

The media player is an example for 1.) while the tray icon is an example for 3.)
If you find you need a new plugin interface (called Activatables) in the code 
feel free to contact us on the mailing list. In general such a tight coupling
should be avoided.

About the exposed GIR API: At the moment there is no stable API. Its just some
header files fed into g-ir-scanner. Despite this method names of the core
functionality in Liferea has proven to be stable during release branches. And
if you contribute your plugin upstream it will be updated to match renamed
functionality.


###### Testing Plugins

To test your new plugin you can use ~/.local/share/liferea/plugins. Create 
the directory and put the plugin script and the .plugin file there and restart
Liferea.

Watch out for initialization exceptions on the command line as they will
permanently disable the plugin. Each time this happens you need to reenable
the plugin from within the plugin tab in the preferences dialog!


###### _How to Help With Testing_

###### *Bug Reports*

If you want to help with testing grab the latest tarball or follow GIT master
and write bug reports for any functional problem you experience. If you have
time help with bug triaging. Check if you see any of the open bugs that are
not yet confirmed.


###### *Debugging Crashes*

In case of crashes create gdb backtraces and post them in the bug tracker. To
create a backtrace start Liferea using "gdb liferea". At the gdb prompt type
"run" to start the execution and "bt" after the crash. Send us the "bt" output!

Note: Often people confuses assertions with crashes. Assertions do halt the
program because of a totally unexpected situation. Creating a backtrace in this
situation will only point to the assertion line, which doesn't help much. In case
of an assertion simply post a bug report with the assertion message.


###### *Debugging Memory Leaks*

If you see memory leakage please take the time to do a run 

    valgrind --leak-check=full liferea

to identify leaks and send in the output.


How to Get Support
------------------

## When using distribution packages

Do not post bug reports in the Liferea bug tracker, use the bug reporting
system of your distribution instead. We (upstream) cannot fix distribution
packages!

## Before raising an issue

Install the latest stable release and check if the problem is solved already.
Please do not ask for help for older releases!

## Issue Tracker

Once you verified the latest stable release still has the problem
please raise an issue in the GitHub bug tracker
(https://github.com/lwindolf/liferea/issues).
