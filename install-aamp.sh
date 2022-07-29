#!/bin/bash
# This script will setup basic environment and fetch aamp code
# for a vanilla Big Sur/Monterey system to be ready for development
#######################Default Values##################
aamposxinstallerver="0.11"
defaultbuilddir=aamp-devenv-$(date +"%Y-%m-%d-%H-%M")
defaultcodebranch="dev_sprint_22_1"
defaultchannellistfile="$HOME/aampcli.csv"
defaultopensslversion="openssl@1.1"

arch=$(uname -m)
echo "Architecture is +$(arch)+"
if [[ $arch == "x86_64" ]]; then
    defaultgstversion="1.18.6"
elif [[ $arch == "arm64" ]]; then
    defaultgstversion="1.20.3" #1.19.90
else
    echo "Achitecture $arch is unsupported"
    exit
fi
######################################################## 
# Declare a status array to collect full summary to be printed by the end of execution
declare -a  arr_install_status

find_or_install_pkgs() {
    # Check if brew package $1 is installed
    # http://stackoverflow.com/a/20802425/1573477
    for pkg in "$@"; 
    do  
    	if brew ls --versions $pkg > /dev/null; then        
            echo "${pkg} is already installed."
            arr_install_status+=("${pkg} is already installed.")
        else
            echo "Installing ${pkg}"
            brew install $pkg
            #update summery
            if brew ls --versions $pkg > /dev/null; then
                #The package is successfully installed 
                arr_install_status+=("The package was ${pkg} was successfully installed.")
            
            else
                #The package is failed to be installed
                arr_install_status+=("The package ${pkg} was FAILED to be installed.")
            fi
        fi
        #if pkg is openssl and its successfully installed every time ensure to symlink to the latest version 
        if [ $pkg =  "${defaultopensslversion}" ]; then
            OPENSSL_PATH=$(brew info $defaultopensslversion|sed '4q;d'|cut -d " " -f1)
            sudo rm -f /usr/local/ssl
            sudo ln -s $OPENSSL_PATH /usr/local/ssl
            export PKG_CONFIG_PATH="$OPENSSL_PATH/lib/pkgconfig" 
        fi
    done
}


install_system_packages() {

    #Install XCode Command Line Tools
    base_macOSver=10.15
    ver=$(sw_vers | grep ProductVersion | cut -d':' -f2 | tr -d ' ')
    if [ $(echo -e $base_macOSver"\n"$ver | sort -V | tail -1) == "$base_macOSver" ];then
        echo "Install XCode Command Line Tools"
        xcode-select --install
        sudo installer -pkg /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_$ver.pkg -target /

    else
        echo "New version of Xcode detected - XCode Command Line Tools Install not required"
        xcrun --sdk macosx --show-sdk-path
        ### added fix for  https://stackoverflow.com/questions/17980759/xcode-select-active-developer-directory-error
        sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
    fi

    #Check/Install base packages needed by aamp env
    echo "Check/Install aamp development environment base packages"
    find_or_install_pkgs  json-glib cmake $defaultopensslversion libxml2 ossp-uuid cjson gnu-sed meson ninja pkg-config

    git clone https://github.com/DaveGamble/cJSON.git
    cd cJSON
    mkdir build
    cd build
    cmake ../
    make
    sudo make install
    cd ../../

    #Install Gstreamer and plugins if not installed
    if [ -f  /Library/Frameworks/GStreamer.framework/Versions/1.0/bin/gst-launch-1.0 ];then
        if [ $(/Library/Frameworks/GStreamer.framework/Versions/1.0/bin/gst-launch-1.0 --version | head -n1 |cut -d " " -f 3) == $defaultgstversion ] ; then
            echo "gstreamer ver $defaultgstversion installed"
            return
        fi
    fi
    echo "Installing GStreamer packages..."
    #brew remove -f  --ignore-dependencies gstreamer gst-validate gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-validate gst-libav gst-devtools
    #sudo rm -rf /usr/local/lib/gstreamer-1.0
    #curl -o gstreamer-1.0-1.18.6-x86_64.pkg  https://gstreamer.freedesktop.org/data/pkg/osx/1.18.6/gstreamer-1.0-1.18.6-x86_64.pkg
    #sudo installer -pkg gstreamer-1.0-1.18.6-x86_64.pkg -target /
    #rm gstreamer-1.0-1.18.6-x86_64.pkg
    #curl -o gstreamer-1.0-devel-1.18.6-x86_64.pkg https://gstreamer.freedesktop.org/data/pkg/osx/1.18.6/gstreamer-1.0-devel-1.18.6-x86_64.pkg
    #sudo installer -pkg gstreamer-1.0-devel-1.18.6-x86_64.pkg  -target /
    #rm gstreamer-1.0-devel-1.18.6-x86_64.pkg
    
    if [[ $arch == "x86_64" ]]; then
        curl -o gstreamer-1.0-$defaultgstversion-x86_64.pkg  https://urldefense.com/v3/__https://gstreamer.freedesktop.org/data/pkg/osx/$defaultgstversion/gstreamer-1.0-$defaultgstversion-x86_64.pkg
        sudo installer -pkg gstreamer-1.0-$defaultgstversion-x86_64.pkg -target /
        rm gstreamer-1.0-$defaultgstversion-x86_64.pkg
        curl -o gstreamer-1.0-devel-$defaultgstversion-x86_64.pkg https://urldefense.com/v3/__https://gstreamer.freedesktop.org/data/pkg/osx/$defaultgstversion/gstreamer-1.0-devel-$defaultgstversion-x86_64.pkg
        sudo installer -pkg gstreamer-1.0-devel-$defaultgstversion-x86_64.pkg  -target /
        rm gstreamer-1.0-devel-$defaultgstversion-x86_64.pkg
    elif [[ $arch == "arm64" ]]; then
        curl -o gstreamer-1.0-$defaultgstversion-universal.pkg  https://urldefense.com/v3/__https://gstreamer.freedesktop.org/data/pkg/osx/$defaultgstversion/gstreamer-1.0-$defaultgstversion-universal.pkg
        sudo installer -pkg gstreamer-1.0-$defaultgstversion-universal.pkg -target /
        rm gstreamer-1.0-$defaultgstversion-universal.pkg
        curl -o gstreamer-1.0-devel-$defaultgstversion-universal.pkg https://urldefense.com/v3/__https://gstreamer.freedesktop.org/data/pkg/osx/$defaultgstversion/gstreamer-1.0-devel-$defaultgstversion-universal.pkg
        sudo installer -pkg gstreamer-1.0-devel-$defaultgstversion-universal.pkg  -target /
        rm gstreamer-1.0-devel-$defaultgstversion-universal.pkg
    fi
    
 }

#main/start
echo "Ver=$aamposxinstallerver"
#Optional Command-line support for -b <aamp code branch> and -d <build directory> 
while getopts ":d:b:" opt; do
  case ${opt} in
    d ) # process option d install base directory name
	builddir=${OPTARG}
	echo "${OPTARG}"
      ;;
    b ) # process option b code branch name
	codebranch=${OPTARG}
      ;;
    * ) echo "Usage: $0 [-b aamp branch name] [-d local setup directory name]"
	 exit
      ;;
  esac
done

#Check and if needed setup default aamp code branch name and local environment directory name
if [[ $codebranch == "" ]]; then
	codebranch=${defaultcodebranch}
	echo "using default code branch: $defaultcodebranch"	
fi 

if [[ "$builddir" == "" ]]; then
builddir=$defaultbuilddir
if [ -d "../aamp" ]; then
        abs_path="$(cd "../aamp" && pwd -P)"
        while true; do
        read -p '[!Alert!] Install script identified that the aamp folder already exists @ ../aamp.
        Press Y, if you want to use same aamp folder (../aamp) for your simulator build.
        Press N, If you want to use separate build folder for aamp simulator. Press (Y/N)'  yn
                case $yn in
                     [Yy]* ) builddir=$abs_path; echo "using following aamp build directory $builddir"; break;;
                     [Nn]* ) builddir=${defaultbuilddir} ; echo "using following aamp build directory $PWD/$builddir"; break ;;
                     * ) echo "Please answer yes or no.";;
                esac
        done
fi
fi

if [[ "$OSTYPE" == "darwin"* ]]; then

    #Check/Install brew
    which -s brew
    if [[ $? != 0 ]] ; then
        echo "Installing Homebrew, as its not available"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    else
        echo "Homebrew is installed now just updating it"
        brew update
    fi

    brew install git
    brew install cmake

elif [[ "$OSTYPE" == "linux"* ]]; then
    sudo apt install git cmake
fi

if [[ ! -d "$builddir" ]]; then
    echo "Creating aamp build directory under $builddir";
    mkdir $builddir
    cd $builddir
    
    git clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/aamp
    cd aamp
else
    cd $builddir

fi

#build all aamp supporting packages under lib folder 
mkdir -p build

echo "Builddir: $builddir"
echo "Code Branch: $codebranch"





#Check OS=macOS
if [[ "$OSTYPE" == "darwin"* ]]; then

    echo $OSTYPE


    #Check if Xcode is installed
    if xpath=$( xcode-select --print-path ) &&
      test -d "${xpath}" && test -x "${xpath}" ; then
      echo "Xcode already installed. Skipping."

    else
      echo "Installing Xcode…"
      xcode-select --install
      if xpath=$( xcode-select --print-path ) &&
            test -d "${xpath}" && test -x "${xpath}" ; then
            echo "Xcode installed now."
       else
            #... isn't correctly installed
            echo "Xcode installation not detected. Exiting $0 script."
            echo "Xcode installation is mandatory to use this AAMP installation automation script."
            echo "please check your app store for Xcode or check at https://developer.apple.com/download/"
            exit 0
       
       fi
    fi




    #cleanup old libs builds
    if [ -d "./.libs" ]; then
	    sudo rm -rf ./.libs
    fi
    
    mkdir .libs
    cd .libs
	
    install_system_packages
     
    #Build aamp dependent modules
    echo "git clone and install aamp dependencies"
	
    echo "Fetch,aamp custom patch(qtdemux),build and install gst-plugins-good-$defaultgstversion.tar.xz ..." 
    pwd
    curl -o gst-plugins-good-$defaultgstversion.tar.xz https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-$defaultgstversion.tar.xz
    tar -xvzf gst-plugins-good-$defaultgstversion.tar.xz
    cd gst-plugins-good-$defaultgstversion
    pwd
    patch -p1 < ../../OSx/patches/0009-qtdemux-aamp-tm_gst-1.16.patch
    patch -p1 < ../../OSx/patches/0013-qtdemux-remove-override-segment-event_gst-1.16.patch
    patch -p1 < ../../OSx/patches/0014-qtdemux-clear-crypto-info-on-trak-switch_gst-1.16.patch
    patch -p1 < ../../OSx/patches/0021-qtdemux-aamp-tm-multiperiod_gst-1.16.patch
    sed -in 's/gstglproto_dep\x27], required: true/gstglproto_dep\x27], required: false/g' meson.build
    meson --pkg-config-path /Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig build 
    ninja -C build
    ninja -C build install
    sudo cp  /usr/local/lib/gstreamer-1.0/libgstisomp4.dylib /Library/Frameworks/GStreamer.framework/Versions/1.0/lib/gstreamer-1.0/libgstisomp4.dylib
    pwd
    cd ../

    echo "Install libdash"
    sudo rm -rf /usr/local/include/libdash
    mkdir temp
    cp ../install_libdash.sh ./temp
    cd temp

    sudo bash ./install_libdash.sh
    cd ..

    pwd
    git clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/aampabr
    git clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/gst-plugins-rdk-aamp
    git clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/aampmetrics

    #Build aamp components
    echo "Building following aamp components"
 	 
    #Build aampabr
    echo "Building aampabr..."
    pwd
    cd aampabr
    mkdir build
    cd build
    cmake ..
    make
    sudo make install
    cd ../..

    #Build aampmetrics
    echo "Building aampmetrics..."
    cd aampmetrics
    mkdir build
    cd build
    cmake .. 
    make
    sudo make install
    cd ../..


    #Build aamp-cli
    echo "Build aamp-cli"
    pwd
    cd ../
    
    #clean existing build folder if exists
    if [ -d "./build" ]; then
       sudo rm -rf ./build
    fi

    mkdir -p build
    
    cd build && PKG_CONFIG_PATH=/usr/local/opt/ossp-uuid/lib/pkgconfig:/usr/local/opt/libffi/lib/pkgconfig:/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:/usr/local/ssl/lib/pkgconfig:/usr/local/opt/curl/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH cmake -DCMAKE_OSX_SYSROOT="/" -DCMAKE_OSX_DEPLOYMENT_TARGET="" -DSMOKETEST_ENABLED=ON -G Xcode ../

    echo "Please Start XCode, open aamp/build/AAMP.xcodeproj project file"
	
    ##Create default channel ~/aampcli.csv – supports local configuration overrides
    echo "If not present, Create $HOME/aampcli.csv to suport virtual channel list of test assets that could be loaded in aamp-cli"

    if [ -f "$defaultchannellistfile" ]; then
    	echo "$defaultchannellistfile exists."
    else 
    	echo "$defaultchannellistfile does not exist. adding default test version"
	cp ../OSX/aampcli.csv $defaultchannellistfile
    fi	

    if [ -d "AAMP.xcodeproj" ]; then 
        echo "AAMP Environment Sucessfully Installed."
        arr_install_status+=("AAMP Environment Sucessfully Installed.")
    else
	    echo "AAMP Environment FAILED to Install."
        arr_install_status+=("AAMP Environment FAILED to Install.")
    fi
    
    echo "Starting Xcode, open aamp/build/AAMP.xcodeproj project file OR Execute ./aamp-cli or /playbintest <url> binaries"
    echo "Opening AAMP project in Xcode..."

    (sleep 20 ; open AAMP.xcodeproj) &
    
    echo "Now Building aamp-cli" 
    xcodebuild -scheme aamp-cli  build

    if [  -f "./Debug/aamp-cli" ]; then 
        echo "OSX AAMP Build PASSED"
        arr_install_status+=("OSX AAMP Build PASSED")
    else
        echo "OSX AAMP Build FAILED"
        arr_install_status+=("OSX AAMP Build FAILED")
	fi
   
    echo ""   
    echo "********AAMP install summary start************"

    for item in "${!arr_install_status[@]}"; 
    do
        printf "$item ${arr_install_status[$item]} \n"
    done   
    echo ""
    echo "********AAMP install summary end************"    
    #Launching aamp-cli
    
    otool -L ./Debug/aamp-cli
    
   ./Debug/aamp-cli https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd
   
   

    exit 0

    #print final status of the script
elif [[ "$OSTYPE" == "linux"* ]]; then
    
    sudo apt install ninja-build
    
    cd Linux
    #cat ../../../aampmetrics.patch > patches/aampmetrics.patch
    sudo apt-get install freeglut3-dev build-essential
    sudo apt-get install libglew-dev
    sudo bash install-linux-deps.sh
    bash install-linux.sh -b $codebranch
    
    cd ../
    echo "Building aamp-cli..."
    if [ -d "./build" ]; then
       sudo rm -rf ./build
    fi

    mkdir -p build
    
    PKG_CONFIG_PATH=$PWD/Linux/lib/pkgconfig /usr/bin/cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=$PWD/Linux -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_LIBRARY_PATH=$PWD/Linux/lib -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DSMOKETEST_ENABLED=ON -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S$PWD -B$PWD/build -G "Unix Makefiles"
    
    echo "Making aamp-cli..."
    cd build
    make
    sudo make install

    
    if [  -f "./aamp-cli" ]; then
        echo "****Linux AAMP Build PASSED****"
        lld ./aamp-cli
        arr_install_status+=("Linux AAMP Build PASSED")
        echo "Installing VSCode..."
	    sudo snap install --classic code
	    
	    echo "Installing VSCode Dependencies..."
	    code --install-extension ms-vscode.cmake-tools
		
	    echo "Openning VSCode Workspace..."
	    code ../ubuntu-aamp-cli.code-workspace
    else
        echo "****Linux AAMP Build FAILED****"
        arr_install_status+=("Linux AAMP Build FAILED")
    fi
    
    
else

        #abort the script if its not macOS
        echo "Aborting unsupported OS detected"
        echo $OSTYPE
        exit 1
fi
