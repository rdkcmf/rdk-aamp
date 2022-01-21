#!/bin/bash

echo $OSTYPE
aampdir=$PWD/..
echo $aampdir
builddir=$aampdir/Linux
codebranch=dev_sprint_22_1

function clone_rdk_repo() {
    if [ -d $1 ]; then
        echo "exist $1"
    else
        git clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/$1
    fi
}

mkdir -p $builddir
cd $builddir
echo "Builddir: $builddir"

#### CLONE_PACKAGES
clone_rdk_repo aampabr
clone_rdk_repo gst-plugins-rdk-aamp
clone_rdk_repo aampmetrics

if [ -d "cJSON" ]; then
    echo "exist cJSON"
else
    git clone https://github.com/DaveGamble/cJSON
fi

if [ -d "aampmetrics" ]; then
    echo "Patching aampmetrics CMakeLists.txt"
    pushd aampmetrics
    	patch < ../patches/aampmetrics.patch
    popd
else
    echo "aampmetrics not cloned properly"
fi

#### CLONE_PACKAGES

#  TODO: check
# apt-get install gstreamer gst-validate gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-validate gst-libav

# curl -o	gst-plugins-good-1.18.4.tar.xz https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-1.18.4.tar.xz
# tar -xf gst-plugins-good-1.18.4.tar.xz
# cd gst-plugins-good-1.18.4
# pwd
# patch -p1 < $aampdir/local-setup/patches/0009-qtdemux-aamp-tm_gst-1.16.patch	
# patch -p1 < $aampdir/local-setup/patches/0013-qtdemux-remove-override-segment-event_gst-1.16.patch
# patch -p1 < $aampdir/local-setup/patches/0014-qtdemux-clear-crypto-info-on-trak-switch_gst-1.16.patch	
# patch -p1 < $aampdir/local-setup/patches/0021-qtdemux-aamp-tm-multiperiod_gst-1.16.patch 
# meson build
#     ninja -C build
#     ninja -C build install
# cd ..

### Install libdash
if [ -d "libdash" ]; then
    echo "libdash installed"
    libdash_build_dir=$builddir/libdash/libdash/build/
else
    echo "Installing libdash"
    mkdir tmp
    pushd tmp
        echo $PWD
        source $aampdir/install_libdash.sh
        libdash_build_dir=$PWD
    pushd
    rm -rf tmp
fi
### Install libdash

function build_repo()
{
    echo "Building $1 "
    pushd $1
        mkdir -p build
        cd build
        env PKG_CONFIG_PATH=$builddir/lib/pkgconfig cmake .. -DCMAKE_LIBRARY_PATH=$builddir/lib -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_INSTALL_PREFIX=$builddir
        make
        make install
    popd
}

# export env PKG_CONFIG_PATH=$builddir/lib/pkgconfig

build_repo cJSON
build_repo aampabr
build_repo aampmetrics


#### COPY LIBDASH FILES
pushd $libdash_build_dir
    cp ./bin/libdash.so $builddir/lib/
    mkdir -p $builddir/include/libdash
    mkdir -p $builddir/include/libdash/xml
    mkdir -p $builddir/include/libdash/mpd
    mkdir -p $builddir/include/libdash/helpers
    mkdir -p $builddir/include/libdash/network
    mkdir -p $builddir/include/libdash/portable
    mkdir -p $builddir/include/libdash/metrics
    cp -pr ../libdash/include/*.h $builddir/include/libdash
    cp -pr ../libdash/source/xml/*.h $builddir/include/libdash/xml
    cp -pr ../libdash/source/mpd/*.h $builddir/include/libdash/mpd
    cp -pr ../libdash/source/network/*.h $builddir/include/libdash/network
    cp -pr ../libdash/source/portable/*.h $builddir/include/libdash/portable
    cp -pr ../libdash/source/helpers/*.h $builddir/include/libdash/helpers
    cp -pr ../libdash/source/metrics/*.h $builddir/include/libdash/metrics
popd
echo -e 'prefix='$builddir'/lib \nexec_prefix='$builddir' \nlibdir='$builddir'/lib \nincludedir='$builddir'/include/libdash \n \nName: libdash \nDescription: ISO/IEC MPEG-DASH library \nVersion: 3.0 \nRequires: libxml-2.0 \nLibs: -L${libdir} -ldash \nLibs.private: -lxml2 \nCflags: -I${includedir}' > $builddir/lib/pkgconfig/libdash.pc

echo "AAMP Workspace Sucessfully prepared" 
echo "Please Start VS Code, open workspace from file: ubuntu-aamp-cli.code-workspace"
