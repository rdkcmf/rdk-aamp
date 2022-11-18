AAMP Library Mocks
------------------

AAMP can be built with support libraries replaced by mocks and stubs. As well as
for test development, these mocks and stubs can be used to check for AAMP
compile time issues using the build host compiler. Header files from an RDK build
or SDK can be used for a mock library AAMP build.

The mocks use the GoogleTest mock library. See:

https://google.github.io/googletest/

Install GoogleTest mocking on the build host. For example, for Ubuntu:

sudo apt-get install libgmock-dev

Install DirectFB to build AAMP with stubbed RDK CC Manager support. For example,
for Ubuntu:

sudo apt-get install libdirectfb-dev

On Ubuntu, install-aamp.sh can be modified to install the required packages and
build AAMP with mock libraries.

Options
-------

The following CMAKE defines enable mocks and stubs when building AAMP:

CMAKE_USE_OPENCDM_ADAPTER_MOCKS

Use with CMAKE_USE_OPENCDM_ADAPTER and CMAKE_USE_THUNDER_OCDM_API_0_2 for mock
WPEFramework/Thunder Open CDM DRM support.

CMAKE_IARM_MGR_MOCKS

Use with CMAKE_CDM_DRM or CMAKE_USE_OPENCDM_ADAPTER along with CMAKE_IARM_MGR
for mock IARM Bus and related support.

CMAKE_USE_ION_MEMORY_MOCKS

Use with CMAKE_USE_OPENCDM_ADAPTER, CMAKE_USE_OPENCDM_ADAPTER_MOCKS,
CMAKE_USE_THUNDER_OCDM_API_0_2, CMAKE_USE_VGDRM and CMAKE_USE_ION_MEMORY for
mock Realtek Ion memory support. Set REALTEK_INCDIR to the same path as
STAGING_INCDIR if required to find library header files.

CMAKE_USE_RFC_MOCKS

Use with CMAKE_CDM_DRM or CMAKE_USE_OPENCDM_ADAPTER along with
CMAKE_AAMP_RFC_REQUIRED for mock TR-181 device data model support.

CMAKE_USE_SECCLIENT_MOCKS

Use with CMAKE_USE_OPENCDM_ADAPTER, CMAKE_USE_OPENCDM_ADAPTER_MOCKS,
CMAKE_USE_THUNDER_OCDM_API_0_2 and CMAKE_USE_SECCLIENT for mock security client
support

CMAKE_USE_PLAYREADY_MOCKS

Use with CMAKE_CDM_DRM and CMAKE_USE_PLAYREADY for stubbed legacy PlayReady
library support.

CMAKE_USE_SEC_API_MOCKS

Use with CMAKE_CDM_DRM and SECAPI_ENGINE_BROADCOM_SAGE to disable linking
SecApi library support.

CMAKE_USE_CC_MANAGER_MOCKS

Use with CMAKE_RDK_CC_ENABLED or CMAKE_SUBTEC_CC_ENABLED for stubbed RDK CC
Manager support. DirectFB is needed for this build.

Building AAMP
-------------

When building AAMP using the build host compiler, library header files from an
RDK build or SDK can be used. The root directory of the header files is
specified using STAGING_INCDIR. For example, to build AAMP with mock IARM Bus
and WPEFramework/Thunder Open CDM support on Ubuntu using header files from a
skyxione platform RDK build in directory ~/rdk:

PKG_CONFIG_PATH=$PWD/Linux/lib/pkgconfig /usr/bin/cmake \
 --no-warn-unused-cli \
 -DSTAGING_INCDIR=~/rdk/build-skyxione/tmp/sysroots/skyxione/usr/include \
 -DCMAKE_INSTALL_PREFIX=$PWD/Linux \
 -DCMAKE_IARM_MGR=1 \
 -DCMAKE_IARM_MGR_MOCKS=1 \
 -DCMAKE_USE_OPENCDM_ADAPTER=1 \
 -DCMAKE_USE_OPENCDM_ADAPTER_MOCKS=1 \
 -DCMAKE_USE_THUNDER_OCDM_API_0_2=1 \
 -DCMAKE_PLATFORM_UBUNTU=1 \
 -DCMAKE_LIBRARY_PATH=$PWD/Linux/lib \
 -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
 -DCMAKE_BUILD_TYPE:STRING=Debug \
 -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc \
 -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ \
 -S$PWD \
 -B$PWD/build/mock \
 -G "Unix Makefiles"
make -C build/mock

Running mock library builds of AAMP is not supported.
