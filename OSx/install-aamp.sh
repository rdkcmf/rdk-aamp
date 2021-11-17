#!/bin/bash
# This script will setup basic envirnment and fetch aamp code
# for a vanilla Big Sur system to be ready for development
#######################Default Values##################
defaultbuilddir=aamp-devenv-$(date +"%Y-%m-%d-%H-%M")
defaultcodebranch="dev_sprint_21_1"
defaultchannellistfile="$HOME/aampcli.csv"
######################################################## 
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
   	builddir=${defaultbuilddir}
	echo "using default build directory $builddir"
fi

mkdir $builddir
cd $builddir

echo "Builddir: $builddir"
echo "Code Branch: $codebranch"

#Check if Xcode is installed 
if type xcode-select >&- && xpath=$( xcode-select --print-path ) &&
   test -d "${xpath}" && test -x "${xpath}" ; then
   #... is correctly installed
   echo "installed"
else
   echo "Xcode installation not detected. Exiting $0 script." 
   echo "Xcode installation is mandatory to use this AAMP installation automation script."
   echo "please check your app store for Xcode or check at https://developer.apple.com/download/"
   exit 0
   #... isn't correctly installed
fi

#Check OS=macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
        #Mac OSX
        echo $OSTYPE
	
	#Check/Install brew
	which -s brew
	if [[ $? != 0 ]] ; then
	  echo "Installing Homebrew, as its not available"
          /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"	
	else
    		echo "Hoebrew is installed now just updating it"
		brew update
	fi
        
	#Check/Install base packages needed by aamp env
	echo "Check/Install aapm development enviroment base pacakges"

	#Install XCode Command Line Tools
	base_macOSver=10.15
        ver=$(sw_vers | grep ProductVersion | cut -d':' -f2 | tr -d ' ')
        if [ $(echo -e $base_macOSver"\n"$ver | sort -V | tail -1) == "$base_macOSver" ]
        then
		echo "Install XCode Command Line Tools"
                xcode-select --install
                sudo installer -pkg /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_$ver.pkg -target /

        else
                echo "New version of Xcode detected - XCode Command Line Tools Install not required"
                xcrun --sdk macosx --show-sdk-path
		### added fix for  https://stackoverflow.com/questions/17980759/xcode-select-active-developer-directory-error
		sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
        fi

	#Install git
	if brew ls --versions cmake > /dev/null; then
              echo "git already installed."
        else
          echo "Installing git, as its not available"
          brew install git
        fi

	#Install cmake
	if brew ls --versions cmake > /dev/null; then
              echo "cmake already installed."
        else
	  echo "Installing cmake, as its not available"
          brew install cmake
	  ln -s /Applications/CMake.app/Contents/bin/cmake /usr/local/bin/cmake	
	fi

	#Install Openssl
	if brew ls --versions openssl > /dev/null; then
		echo "openssl already installed."
		#brew reinstall openssl
        else

		echo "Installing openssl, as its not available"
		brew install openssl
                sudo ln -s /usr/local/Cellar/openssl\@1.1/1.1.1l_1 /usr/local/ssl
	fi
	
	#Install libxml
	if brew ls --versions libxml2 > /dev/null; then
                echo "libxml2 already installed."
        else

                echo "Installing libxml2, as its not available"
                brew install libxml2
		ln -s /usr/local/opt/libxml2/lib/pkgconfig/* /usr/local/lib/pkgconfig/
        fi
	
	#Install libuuid
        if brew ls --versions ossp-uuid > /dev/null; then
                echo "ossp-uuid already installed."
        else
                 echo "Installing oosp-uuid, as its not available"
                 brew install ossp-uuid
        fi 

        #Install cjson	
        if brew ls --versions cjson > /dev/null; then
                echo "cjson already installed."
        else

                echo "Installing cjson, as its not available"
                brew install cjson
        fi

	#Install gnu-sed
        if brew ls --versions gnu-sed > /dev/null; then
                echo "gnu-sed already installed."
        else

                echo "Installing gnu-sed, as its not available"
                brew install gnu-sed
        fi


   	#Install meson
        if brew ls --versions meson > /dev/null; then
                echo "meson already installed."
        else

                echo "Installing meson, as its not available"
                brew install meson
        fi

 	#Install ninja
        if brew ls --versions ninja > /dev/null; then
                echo "ninja already installed."
        else

                echo "Installing ninja, as its not available"
                brew install ninja
        fi


        #Build aamp and dependent modules
        echo "git clone aamp and dependent components in a new directory"

        git clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/aamp
        git clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/aampabr
        git clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/gst-plugins-rdk-aamp
        git clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/aampmetrics

        #Install Gstreamer and plugins
        echo "Installing GStreamer packages..."
        brew install gstreamer gst-validate gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-validate gst-libav

        #Patch qtdemux plugin + Build and Install gstreamer 1.18.4 plugins
	 
	echo "Fetch,aamp custom patch(qtdemux),build and install gst-plugins-good-1.18.4.tar.xz ..."

        curl -o	gst-plugins-good-1.18.4.tar.xz https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-1.18.4.tar.xz
 	tar -xvzf gst-plugins-good-1.18.4.tar.xz
	cd gst-plugins-good-1.18.4
	pwd
	patch -p1 < ../aamp/OSx/patches/0009-qtdemux-aamp-tm_gst-1.16.patch	
	patch -p1 < ../aamp/OSx/patches/0013-qtdemux-remove-override-segment-event_gst-1.16.patch
	patch -p1 < ../aamp/OSx/patches/0014-qtdemux-clear-crypto-info-on-trak-switch_gst-1.16.patch	
	patch -p1 < ../aamp/OSx/patches/0021-qtdemux-aamp-tm-multiperiod_gst-1.16.patch 
 	meson build
    	ninja -C build
    	ninja -C build install
	cd ..

	#Install libdash
	echo "Install libdash"
        cd aamp
        source ./install_libdash.sh
	cd ../../../

	#Build aamp components
	echo "Building following aamp components"
	gsed -i '525,525 s:^://:' aampabr/ABRManager.cpp

 	
	#Build aampabr
	echo "Building aampabr..."
	pwd
	cd aampabr
	mkdir build
	cd build
	cmake ../
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
	cd aamp
	
	awk '{sub("/Applications/CMake.app/Contents/bin/cmake","/usr/local/bin/cmake")}1' osxbuild.sh  > temp.txt && mv temp.txt osxbuild.sh
	export PKG_CONFIG_PATH="/usr/local/opt/openssl@1.1/lib/pkgconfig"
	pwd
	
	bash osxbuild.sh
	echo "Please Start XCode, open aamp/build/AAMP.xcodeproj project file"
	
	##Create default channel ~/aampcli.csv – supports local configuration overrides
	echo "If not present, Create $HOME/aampcli.csv to suport virtual channel list of test assets that could be loaded in aamp-cli"

	if [ -f "$defaultchannellistfile" ]; then
    		echo "$defaultchannellistfile exists."
	else 
    		echo "$defaultchannellistfile does not exist. adding default test version"
		cp ./OSx/aampcli.csv $defaultchannellistfile
	fi
	
	echo "AAMP Sucessfully Installed." 
	echo "Please Start Xcode, open aamp/build/AAMP.xcodeproj project file OR Execute ./aamp-cli or /playbintest <url> binaries"
	echo "Opening AAMP project in Xcode..."
	open build/AAMP.xcodeproj 
	 	
else

        #abort the script if its not macOS
        echo "Aborting unsupported OS  detected"
        echo $OSTYPE
        exit
fi
