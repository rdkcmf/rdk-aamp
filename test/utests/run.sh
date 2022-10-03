#!/bin/bash
# This script will build and run microtests
mkdir build

cd build

if [[ "$OSTYPE" == "darwin"* ]]; then
    PKG_CONFIG_PATH=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH cmake ../
elif [[ "$OSTYPE" == "linux"* ]]; then
    PKG_CONFIG_PATH=$PWD/../../../Linux/lib/pkgconfig /usr/bin/cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=$PWD/../../../Linux -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_LIBRARY_PATH=$PWD/../../../Linux/lib -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S../ -B$PWD -G "Unix Makefiles"
else
    #abort the script if its not macOS or linux
    echo "Aborting unsupported OS detected"
    echo $OSTYPE
    exit 1
fi

make

ctest -j 4 --output-on-failure --no-compress-output -T Test

exit $?
