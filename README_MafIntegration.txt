#### AAMP Integration with MAF

The MAF resizer aims at down-sizing any given image into a 'tiny' 8x8-pixels.
This document aims at statically linking maf-frame-resize and maf-hardcut-detector to the AAMP.

####To DO
1.Run the install_maf.sh script in the aamp directory - helps to automatically clone the MAF components and integrate with AAMP.
2.Build the aamp code base with -DCMAKE_USE_MAF=1 .

Preferred system is Ubuntu 18 or later; MacOs also is Ok.

### Build Steps
**1. Clone aamp and dependent components in a new directory**
``` 
git clone -b acrlar_2205 https://code.rdkcentral.com/r/rdk/components/generic/aamp
git clone -b acrlar_2205 https://code.rdkcentral.com/r/rdk/components/generic/aampabr
git clone -b acrlar_2205 https://code.rdkcentral.com/r/rdk/components/generic/gst-plugins-rdk-aamp
git clone -b acrlar_2205 https://code.rdkcentral.com/r/rdk/components/generic/aampmetrics
git clone git://github.com/DaveGamble/cJSON.git
```

**2. Build the components in the following order -**

**a) aampabr**
```
	cd aampabr
	mkdir build
	cd build
	cmake ../ -DCMAKE_BUILD_TYPE=Debug
	make
	sudo make install
	cd ../..
```

**b) cJSON**
```
	cd cJSON
	mkdir build
	cd build
	cmake ../
	make
	sudo make install
	cd ../..
```

**c) aampmetrics**
```
	cd aampmetrics
	mkdir build
	cd build
	cmake ../ -DCMAKE_BUILD_TYPE=Debug
	make
	sudo make install
	cd ../..
```

**d) libdash**
```
	cd aamp
	./install_libdash.sh
    cd ../libdash/libdash/build
	sudo make install
    cd ../../..
   
```

**e) Build aamp**
```
	cd aamp
	./install_maf.sh  #This will automaically clone the MAF components,install the dependencies and integrates MAF with aamp code
	mkdir build
	cd build
	cmake ../ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_USE_MAF=1
	make
	sudo make install
	cd ../../
```

**f) gst-plugins-rdk-aamp**
```
	cd gst-plugins-rdk-aamp
	mkdir build
	cmake ../ -DCMAKE_BUILD_TYPE=Debug
	make
	sudo make install
	cd ../../
```

## Execute binaries
```
	./aamp-cli
```

