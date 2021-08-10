---

# AAMP on Mac OS X

This document contains the instructions to setup and debug stand alone AAMP (aamp-cli) on Mac OS X.

## Install dependancies

**1. Install XCode from the Apple Store**

**2. Install XCode Command Line Tools**

This is required for MacOS version < 10.15

```
xcode-select --install
sudo installer -pkg /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_<version>.pkg -target /
```

For MacOS 10.15 & above, we can check the SDK install path as
```
xcrun --sdk macosx --show-sdk-path
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.sdk
```

**3. Install GStreamer packages**

Install Homebrew, if not available in your Mac:
```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

Install gst packages:
```
brew install gstreamer gst-validate gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-validate gst-libav
```

You can find help [here](https://wesleyli.co/2016/10/running-gstreamer-on-mac-os-x).
More details about packages available at [freedesktop.org](https://gstreamer.freedesktop.org/documentation/installing/on-mac-osx.html)

**3.1 Build and install patched gst-plugins-good**
Optional, but fixes some playback issues.

Dependencies:
```brew install meson ninja```

Download https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-1.18.2.tar.xz
```tar -xvzf gst-plugins-good-1.18.2.tar.xz```

Download the following patches  from RDK Central to gst-plugins-good-1.18.2 directory. Note that the patches are valid for 1.16 or 1.18, but the 1.16 build directions are different and have not been included here.
```https://gerrit.teamccp.com/plugins/gitiles/rdk/yocto_oe/layers/meta-rdk-ext/+/2101_sprint/recipes-multimedia/gstreamer/files/```
```
0009-qtdemux-aamp-tm_gst-1.16.patch
0013-qtdemux-remove-override-segment-event_gst-1.16.patch
0014-qtdemux-clear-crypto-info-on-trak-switch_gst-1.16.patch
0021-qtdemux-aamp-tm-multiperiod_gst-1.16.patch
```
Apply patches
```
cd gst-plugins-good-1.18.2
patch -p1 < file.patch```
```
**Build & Install**
```
meson build
ninja -C build
cd build
ninja test [Should all pass but one.]
sudo ninja install
```
**4. Install [Cmake](https://cmake.org/download/)**

like cmake-3.18.0-Darwin-x86_64.dmg or latest

Set link for CMake command line.
``` ln -s /Applications/CMake.app/Contents/bin/cmake /usr/local/bin```

**5. Install OpenSsl**

```
brew install openssl
brew reinstall openssl		//if already installed
sudo ln -s /usr/local/Cellar/openssl\@1.1/1.1.1g /usr/local/ssl
```
Here 1.1.1g is the version 

**6. Install libXML2**

```
brew install libxml2
ln -s /usr/local/opt/libxml2/lib/pkgconfig/* /usr/local/lib/pkgconfig/
```
**7. Install libdash**

```
source install_libdash.sh
```
or
	
***Build***:

```
git clone git://github.com/bitmovin/libdash.git
cd libdash/libdash
git checkout stable_3_0
```

Apply patches downloaded from (**patch -p1 < file.patch**):
```https://code.rdkcentral.com/r/plugins/gitiles/rdk/components/generic/rdk-oe/meta-rdk-ext/+/rdk-next/recipes-multimedia/libdash/libdash/```

Clone the patch repo
git clone -b rdk-next "https://code.rdkcentral.com/r/rdk/components/generic/rdk-oe/meta-rdk-ext"

Apply patches in below order
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0001-libdash-build.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0002-libdash-starttime-uint64.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0003-libdash-presentationTimeOffset-uint64.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0004-Support-of-EventStream.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0005-DELIA-39460-libdash-memleak.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0006-RDK-32003-LLD-Support.patch

```
mkdir build
cd build
cmake ..
make
```

***Install***:

```
cp bin/libdash.dylib /usr/local/lib/
mkdir /usr/local/include
mkdir /usr/local/include/libdash
mkdir /usr/local/include/libdash/xml
mkdir /usr/local/include/libdash/mpd
mkdir /usr/local/include/libdash/helpers
mkdir /usr/local/include/libdash/network
mkdir /usr/local/include/libdash/portable
mkdir /usr/local/include/libdash/metrics
cp -pr ../libdash/include/*.h /usr/local/include/libdash
cp -pr ../libdash/source/xml/*.h /usr/local/include/libdash/xml
cp -pr ../libdash/source/mpd/*.h /usr/local/include/libdash/mpd
cp -pr ../libdash/source/network/*.h /usr/local/include/libdash/network
cp -pr ../libdash/source/portable/*.h /usr/local/include/libdash/portable
cp -pr ../libdash/source/helpers/*.h /usr/local/include/libdash/helpers
cp -pr ../libdash/source/metrics/*.h /usr/local/include/libdash/metrics

echo -e 'prefix=/usr/local \nexec_prefix=${prefix} \nlibdir=${exec_prefix}/lib \nincludedir=${prefix}/include/libdash \n \nName: libdash \nDescription: ISO/IEC MPEG-DASH library \nVersion: 3.0 \nRequires: libxml-2.0 \nLibs: -L${libdir} -ldash \nLibs.private: -lxml2 \nCflags: -I${includedir}'  > /usr/local/lib/pkgconfig/libdash.pc
```
**8. Install libuuid**

```
brew install ossp-uuid
```

**9. Install cjson**

```
brew install cjson
```

**10. Install aampabr**

```
git clone https://code.rdkcentral.com/r/rdk/components/generic/aampabr aampabr
cd aampabr
mkdir build
cd build
cmake ..
make
make install
```

**11. Install aampmetrics**

```
git clone https://code.rdkcentral.com/r/rdk/components/generic/aampmetrics aampmetrics
cd aampmetrics
mkdir build
cd build
cmake ..
make
make install
```

##Build and execute aamp-cli
**1. Open aamp.xcodeproj in Xcode**

```
git clone "https://code.rdkcentral.com/r/rdk/components/generic/aamp" -b dev_sprint
```

**2. Build the code**

```
	osxbuild.sh
	Start XCode, open build/AAMP.xcodeproj project file
	Product -> Build
```
If you see the error 'No CMAKE_C_COMPILER could be found.' when running osxbuild.sh, check that your installed cmake version matches the minimum required version shown earlier.
Even after updating the CMake version if you still see the above error, then run "sudo xcode-select --reset" and then execute the "osxbuild.sh" this fixes the issue.


**3. Select target to execute**

```
	Product -> Scheme -> Choose Scheme
	aamp-cli
```

**4. Execute**

While executing if you face the below MacOS warning then please follow the below steps to fix it.
"Example warning: Machine runs macOS 10.15.7, which is lower than aamp-cli's minimum deployment target of 11.1. Change your project's minimum deployment target or upgrade machine’s version of macOS."
Click "AAMP" project and lower the "macOS Deployment Target" version. For example: Change it to 10.11 then run the aamp-cli, it will work.


```
Product -> Run
```
