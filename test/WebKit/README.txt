AAMP WebKit
===========

In this directory is the source code for:

aamp-js

    A test utility to run AAMP from JavaScript.

InjectedBundle

    A dynamic library enabling the AAMP reference player to be run in a WebKit
    browser.

These can be built and run on Ubuntu using WebKitGTK, for example using
VirtualBox on a Windows laptop.

AAMP-JS
=======

This test utility allows AAMP to be run from JavaScript files. It is built by
default when AAMP is built with CMAKE_WEBKITGTK_JSBINDINGS set.

The following built-in JavaScript classes are added:

AAMPMediaPlayer

    Class which implements the AAMP media player. See UVEMediaPlayer.js for
    details.

AAMPTestUtils

    Utilities class which implements the following methods:

    sleep(s)

        Sleep for s seconds.

    log(s)

        Print string s on the console.

Build Instructions (Linux)
--------------------------

On Ubuntu, build AAMP (if not already done so):

$ cd aamp
$ bash install-aamp.sh

Install WebKitGTK JavaScriptCore. For example, on Ubuntu:

$ sudo apt update
$ sudo apt install libjavascriptcoregtk-4.0-dev

Alternatively, build WebKitGTK from source as described in this file for the
AAMP WebKit InjectedBundle.

Rebuild AAMP with CMAKE_WEBKITGTK_JSBINDINGS set:

$ PKG_CONFIG_PATH=$PWD/Linux/lib/pkgconfig \
 /usr/bin/cmake --no-warn-unused-cli \
 -DCMAKE_INSTALL_PREFIX=$PWD/Linux \
 -DCMAKE_PLATFORM_UBUNTU=1 \
 -DCMAKE_WEBKITGTK_JSBINDINGS=TRUE \
 -DCMAKE_LIBRARY_PATH=$PWD/Linux/lib \
 -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
 -DCMAKE_BUILD_TYPE:STRING=Debug \
 -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc \
 -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ \
 -S$PWD \
 -B$PWD/build \
 -G "Unix Makefiles"
$ make -C build
$ sudo make -C build install

Alternatively, if Ninja is used to build AAMP:

$ PKG_CONFIG_PATH=$PWD/Linux/lib/pkgconfig \
 /usr/bin/cmake --no-warn-unused-cli \
 -DCMAKE_INSTALL_PREFIX=$PWD/Linux \
 -DCMAKE_PLATFORM_UBUNTU=1 \
 -DCMAKE_WEBKITGTK_JSBINDINGS=TRUE \
 -DCMAKE_LIBRARY_PATH=$PWD/Linux/lib \
 -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
 -DCMAKE_BUILD_TYPE:STRING=Debug \
 -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc \
 -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ \
 -S$PWD \
 -B$PWD/build \
 -G Ninja
$ ninja -C build
$ sudo ninja -C build install

Running AAMP-JS
---------------

For example, put the following into a file test.js in the aamp directory:

    Test = new AAMPTestUtils();

    Test.log("Creating AAMP player");
    player = new AAMPMediaPlayer("test.js");

    url = "http://amssamples.streaming.mediaservices.windows.net/683f7e47-bd83-4427-b0a3-26a6c4547782/BigBuckBunny.ism/manifest(format=mpd-time-csf)";
    Test.log("Loading " + url);
    player.load(url, true);
    Test.sleep(1);

    Test.log("Playing");
    player.play();
    Test.sleep(5);

Run aamp-js:

$ LD_LIBRARY_PATH=./Linux/lib ./Linux/bin/aamp-js test.js

Known Limitations
-----------------

Console operations such as console.log() may not be enabled in certain
distributions of WebKitGTK JavaScriptCore. The JavaScriptCore library can be
built as part of WebKitGTK for the AAMP WebKit InjectedBundle as described in
this file.

Alternatively, use the AAMPTestUtils log() method. For example:

Test = new AAMPTestUtils();
Test.log("Hello World");

Or, for source compatibility:

console = new AAMPTestUtils();
console.log("Hello World");

AAMP WebKit InjectedBundle
==========================

The AAMP WebKit InjectedBundle can be used to test the AAMP reference player in
Ubuntu, for example using VirtualBox on a Windows laptop. The WebKitGTK injected
bundle shared library is replaced by one which adds the AAMPMediaPlayer
JavaScript class. A WebKit browser such as MiniBrowser can then be used to run
the AAMP reference player.

Note:

50GB of disk may be required.

Building WebKitGTK can be slow on systems with few processors.

Build Instructions (Linux)
--------------------------

Install and build AAMP on Ubuntu, for example using install-aamp.sh:

$ cd aamp
$ bash install-aamp.sh

Download a specific version of WebKit. For example WebKitGTK 2.36.7:

$ git clone --depth=1 -b webkitgtk-2.36.7 https://github.com/WebKit/WebKit.git Linux/WebKit

Alternatively:

$ curl -O https://webkitgtk.org/releases/webkitgtk-2.36.7.tar.xz
$ tar xf webkitgtk-2.36.7.tar.xz
$ mv webkitgtk-2.36.7 Linux/WebKit
$ rm webkitgtk-2.36.7.tar.xz

Install WebKitGTK dependencies. These will depend on the WebKitGTK version,
configuration and what is already installed on Ubuntu. For example:

$ sudo apt update
$ sudo apt install -y \
 libtasn1-dev \
 libcairo-dev \
 libgcrypt-dev \
 libharfbuzz-dev \
 libsqlite3-dev \
 libwebp-dev \
 libgtk-3-dev \
 libsoup2.4-dev \
 libxslt-dev \
 libsecret-1-dev \
 libgirepository1.0-dev \
 libtasn1-dev \
 libenchant-dev \
 libhyphen-dev \
 libopenjp2-7-dev \
 libwoff-dev \
 libsystemd-dev \
 liblcms-dev \
 libseccomp-dev \
 ruby \
 gperf \
 gettext

Patch WebKitGTK so that WebKit exports symbols required by the AAMP WebKit
InjectedBundle:

$ patch -p 1 \
 Linux/WebKit/Source/WebKit/webkitglib-symbols.map \
 test/WebKit/webkitgtk-2.36.7.patch

Configure the WebKit build as required. For example:

$ cmake \
 -Bbuild/WebKit \
 -SLinux/WebKit \
 -DCMAKE_INSTALL_PREFIX=./Linux \
 -DCMAKE_BUILD_TYPE=Debug \
 -DPORT=GTK \
 -DENABLE_MINIBROWSER=ON \
 -DUSE_SOUP2=ON \
 -DENABLE_GAMEPAD=OFF \
 -DUSE_LIBNOTIFY=OFF \
 -DENABLE_BUBBLEWRAP_SANDBOX=OFF \
 -DUSE_WPE_RENDERER=OFF

Build WebKitGTK in parallel, based on the number of processors:

$ nproc
4
$ make -j 4 -C build/WebKit

Then install:

$ sudo make -C build/WebKit install

Rebuild and reinstall AAMP, adding JavaScript binding wrappers. For example:

$ PKG_CONFIG_PATH=$PWD/Linux/lib/pkgconfig \
 /usr/bin/cmake \
 --no-warn-unused-cli \
 -DCMAKE_INSTALL_PREFIX=$PWD/Linux \
 -DCMAKE_PLATFORM_UBUNTU=1 \
 -DCMAKE_WEBKITGTK_JSBINDINGS=TRUE \
 -DCMAKE_LIBRARY_PATH=$PWD/Linux/lib \
 -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
 -DCMAKE_BUILD_TYPE:STRING=Debug \
 -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc \
 -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ \
 -S$PWD \
 -B$PWD/build \
 -G "Unix Makefiles"
$ make -C build
$ sudo make -C build install

Build the AAMP InjectedBundle, replacing the WebKitGTK InjectedBundle:

$ g++ -std=c++17 -fPIC --shared \
 -o ./Linux/lib/libwebkit2gtkinjectedbundle.so \
 test/WebKit/AampInjectedBundle.cpp \
 -I ./build/WebKit/DerivedSources/ForwardingHeaders/WebKit \
 -I ./build/WebKit/DerivedSources/ForwardingHeaders \
 -I ./build/WebKit/JavaScriptCore/Headers \
 -I ./Linux/WebKit/Source \
 -I ./Linux/WebKit/Source/WebKit/WebProcess/InjectedBundle/API/c \
 -I /usr/include/gstreamer-1.0 \
 -I /usr/include/glib-2.0 \
 -I /usr/lib/x86_64-linux-gnu/glib-2.0/include \
 -I ./Linux/include/cjson/ \
 -I ./jsbindings \
 -I ./Linux/include \
 -L ./Linux/lib -lwebkit2gtk-4.0 -laampjsbindings -lcjson \
 -L /usr/lib/x86_64-linux-gnu -lgstreamer-1.0 \
 -Wl,--no-undefined
$ sudo install -m 0755 \
 ./Linux/lib/libwebkit2gtkinjectedbundle.so \
 ./Linux/lib/webkit2gtk-4.0/injected-bundle/libwebkit2gtkinjectedbundle.so

Running the AAMP Reference Player
---------------------------------

Run the AAMP reference player using MiniBrowser:

$ LD_LIBRARY_PATH=./Linux/lib \
 ./Linux/libexec/webkit2gtk-4.0/MiniBrowser \
 --enable-write-console-messages-to-stdout=True \
 test/ReferencePlayer/UVE/index.html

Known Limitations
-----------------

Video is displayed in a separate window.

There is limited support for clicking on elements in the reference player test
UI.

There is no implementation of the XREReceiver class.
