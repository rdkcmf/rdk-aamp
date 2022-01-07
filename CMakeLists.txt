# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2018 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
cmake_minimum_required (VERSION 2.6)
project (AAMP)
#find_package(GStreamer 1.4 REQUIRED)
#add_subdirectory(jsbindings)
find_package(PkgConfig REQUIRED)

if( CMAKE_USE_GST_0.10 )
	message( FATAL_ERROR "gstreamer-0.10 not supported" )
else()
	message("using gstreamer-1.0")
	pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
	pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)
endif()

pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(LibXml2 REQUIRED libxml-2.0)
pkg_check_modules(LIBDASH REQUIRED libdash)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(LIBCJSON REQUIRED libcjson)

if (CMAKE_WPEFRAMEWORK_REQUIRED)
    message("CMAKE_WPEFRAMEWORK_REQUIRED set")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
find_package(WPEFramework REQUIRED)
if (WPEFRAMEWORK_FOUND)
    message("WPEFRAMEWORK_FOUND = ${WPEFRAMEWORK_FOUND}")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${WPEFRAMEWORK_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "WPEFrameworkProtocols not found")
endif()
endif()

if(CMAKE_QT5WEBKIT_JSBINDINGS)
	message("CMAKE_QT5WEBKIT_JSBINDINGS set, require Qt5WebKit")
	pkg_check_modules(Qt5WebKit REQUIRED Qt5WebKit)
	find_path (STAGING_INCDIR webkit-apis/ForwardingHeaders)
	include_directories(${STAGING_INCDIR}/webkit-apis/ForwardingHeaders)
elseif(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_QT5WEBKIT_JSBINDINGS not set, CMAKE_WPEWEBKIT_JSBINDINGS is set, Finding JavaScriptCore")
	find_path (STAGING_INCDIR JavaScriptCore)
	include_directories(${STAGING_INCDIR}/JavaScriptCore)
else()
	message("CMAKE_QT5WEBKIT_JSBINDINGS not set, CMAKE_WPEWEBKIT_JSBINDINGS not set")
endif()

if(CMAKE_RENDER_FRAMES_IN_APP_CONTEXT)
        #Gstreamer will export frames to app. It is app's responsibility to render frames.
        message("CMAKE_RENDER_FRAMES_IN_APP_CONTEXT set")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DRENDER_FRAMES_IN_APP_CONTEXT")
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
        set(OPENGL_LIBRARIES "-framework OpenGL -framework GLUT")
else()
       pkg_check_modules(OPENGL REQUIRED gl)
       set(OPENGL_LIBRARIES "${OPENGL_LIBRARIES} -lglut")
       pkg_check_modules(GLEW REQUIRED glew)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
endif()

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    execute_process (
        COMMAND bash -c "xcrun --show-sdk-path" OUTPUT_VARIABLE osxSdkPath OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(OS_CXX_FLAGS "${OS_CXX_FLAGS} -g -x objective-c++ -Wno-inconsistent-missing-override -F${osxSdkPath}/System/Library/Frameworks")
    set(OS_LD_FLAGS "${OS_LD_FLAGS} -F${osxSdkPath}/System/Library/Frameworks -framework Cocoa -L${osxSdkPath}/../MacOSX.sdk/usr/lib -L/usr/local/lib/")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${osxSdkPath}/../MacOSX.sdk -I/usr/local/include")
    string(STRIP ${OS_LD_FLAGS} OS_LD_FLAGS)
    set(AAMP_CLI_LD_FLAGS "${AAMP_CLI_LD_FLAGS} -lgstvideo-1.0")
    string(STRIP ${AAMP_CLI_LD_FLAGS} AAMP_CLI_LD_FLAGS)
    set(AAMP_OS_SOURCES cocoa_window.mm)
    link_directories(${OPENSSL_LIBRARY_DIRS})
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    pkg_check_modules(GLIB REQUIRED GLib-2.0)
    include_directories(${GLIB_INCLUDE_DIRS})
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DENABLE_AAMP_QTDEMUX_OVERRIDE=1 -DCONTENT_4K_SUPPORTED")
else()
    set(USE_MAC_FOR_RANDOM_GEN "-DUSE_MAC_FOR_RANDOM_GEN")
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

if(CMAKE_RDK_CC_ENABLED)
	pkg_check_modules(DIRECTFB REQUIRED directfb)
endif()

find_package (Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} drm drm/ave drm/aes drm/helper subtitle isobmff)
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${GSTREAMERBASE_INCLUDE_DIRS})
include_directories(${LIBDASH_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIRS})
include_directories(${OPENGL_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${LIBCJSON_INCLUDE_DIRS})


set(LIBAAMP_SOURCES iso639map.cpp
                    base16.cpp
                    _base64.cpp
                    AampMemoryUtils.cpp
                    AampCacheHandler.cpp
                    AampScheduler.cpp
                    AampUtils.cpp
                    AampJsonObject.cpp
                    AampProfiler.cpp
                    AampEvent.cpp
                    AampEventListener.cpp
                    ota_shim.cpp
                    hdmiin_shim.cpp
                    videoin_shim.cpp
                    compositein_shim.cpp
                    fragmentcollector_progressive.cpp
                    fragmentcollector_hls.cpp
                    fragmentcollector_mpd.cpp
                    admanager_mpd.cpp
                    streamabstraction.cpp
                    drm/ave/drm.cpp
                    priv_aamp.cpp
                    main_aamp.cpp
                    aampgstplayer.cpp
                    tsprocessor.cpp
                    drm/aes/aamp_aes.cpp
                    aamplogging.cpp
		    AampConfig.cpp
		    AampEventManager.cpp
                    subtitle/webvttParser.cpp
                    isobmff/isobmffbox.cpp
                    isobmff/isobmffbuffer.cpp
                    isobmff/isobmffprocessor.cpp
                    drm/helper/AampDrmHelper.cpp
                    AampGstUtils.cpp
		    MediaStreamContext.cpp
)

if (CMAKE_WPEFRAMEWORK_REQUIRED)
	set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} ThunderAccess.cpp Module.cpp)
endif()

if(CMAKE_USE_SECCLIENT)
	set(SEC_CLIENT_LIB -lSecClient )
endif()
if(CMAKE_USE_SECMANAGER)
	if (NOT CMAKE_WPEFRAMEWORK_REQUIRED)
		message(FATAL_ERROR "CMAKE_USE_SECMANAGER set but CMAKE_WPEFRAMEWORK_REQUIRED not set")
	else()
		set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} drm/AampSecManager.cpp)
	endif()
endif()

set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_VANILLA_AES_SUPPORT ${USE_MAC_FOR_RANDOM_GEN}")

if (CMAKE_PLATFORM_UBUNTU)
	message("CMAKE_PLATFORM_UBUNTU set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUBUNTU=1 -DNO_NATIVE_AV=1 -DENABLE_AAMP_QTDEMUX_OVERRIDE=1 -DCONTENT_4K_SUPPORTED")
endif()

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
  message("AAMP_SIMULATOR_BUILD set")
  set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_SIMULATOR_BUILD=1")
endif()

if (CMAKE_USE_OPENCDM_ADAPTER)
  if (NOT CMAKE_USE_OPENCDM)
    message("Forcing CMAKE_USE_OPENCDM due to CMAKE_USE_OPENCDM_ADAPTER")
    set(CMAKE_USE_OPENCDM TRUE)
  endif()
endif()

if(CMAKE_USE_RDK_PLUGINS)
	message("CMAKE_USE_RDK_PLUGINS set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_PLAYERSINKBIN -DREAD_ESTB_IFACE_FROM_DEVICE_PROPERTIES -DCREATE_PIPE_SESSION_TO_XRE")
endif()

set(LIBAAMP_DEPENDS ${OS_LD_FLAGS} ${GSTREAMERBASE_LIBRARIES} ${GSTREAMER_LIBRARIES} ${CURL_LIBRARIES} ${LIBDASH_LIBRARIES} ${LibXml2_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${AAMP_CLI_LD_FLAGS} -ldl -labr -luuid ${SEC_CLIENT_LIB} -lcjson)
set(PLAYBINTEST_DEPENDS ${OS_LD_FLAGS} ${CMAKE_THREAD_LIBS_INIT} ${GSTREAMER_LIBRARIES})

if (CMAKE_AMLOGIC_SOC)
	message("CMAKE_AMLOGIC_SOC set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DNO_NATIVE_AV=1 -DAMLOGIC -DCONTENT_4K_SUPPORTED")

	# For extracting SEI timecode
	set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lgstvideo-1.0")
endif()

if(CMAKE_QT5WEBKIT_JSBINDINGS)
	message("CMAKE_QT5WEBKIT_JSBINDINGS set, adding sources to libaamp")
	set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} jsbindings/jsbindings.cpp jsbindings/jsutils.cpp jsbindings/jsmediaplayer.cpp jsbindings/jseventlistener.cpp jsbindings/jsevent.cpp)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSUPPORT_JS_EVENTS")
	set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -l${Qt5WebKit_LIBRARIES}")
elseif(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_WPEWEBKIT_JSBINDINGS set, just setting flags")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSUPPORT_JS_EVENTS")
else()
	message("CMAKE_WPEWEBKIT_JSBINDINGS and CMAKE_WPEWEBKIT_JSBINDINGS not set")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar -std=c++11 -Wno-non-virtual-dtor -Werror=effc++ -Wno-psabi")

if(CMAKE_IARM_MGR)
	message("CMAKE_IARM_MGR set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DIARM_MGR=1")
endif()

set(AAMP_CLI_SOURCES test/aampcli.cpp ${AAMP_OS_SOURCES})

set(AAMP_SUBTEC_SOURCES subtec/PacketSender.cpp)

if(CMAKE_BUILD_SUBTEC_TEST)
    if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
        # avoid cmake failure on OSX
            set(CMAKE_MACOSX_RPATH 1)
    else()
        set(AAMP_SUBTEC_TEST_DEFINES "${AAMP_SUBTEC_TEST_DEFINES} -DSUBTEC_PACKET_DEBUG")
        add_executable(subtectest ${AAMP_SUBTEC_SOURCES} subtec/test/subtec_test.cpp)
        set_target_properties(subtectest PROPERTIES COMPILE_FLAGS "${AAMP_SUBTEC_TEST_DEFINES}")
        target_link_libraries(subtectest "-lpthread")
        install(TARGETS subtectest DESTINATION bin)
        message("CMAKE_BUILD_SUBTEC_TEST set")
    endif()
endif()

set(AAMP_SUBTEC_CLASS_SOURCES subtec/WebVttSubtecParser.cpp subtec/TtmlSubtecParser.cpp subtec/WebvttSubtecDevParser.cpp)
set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} ${AAMP_SUBTEC_SOURCES} ${AAMP_SUBTEC_CLASS_SOURCES})
include_directories(subtec)

#To enable session based stats for latency histogram
set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DSESSION_STATS")
set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lmetrics")

if(CMAKE_AVE_DRM)
	message("CMAKE_AVE_DRM set, adding stubs")
	set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -laveadapter -lAVEPlayer -lmetrics")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAVE_DRM")
	set(AAMP_CLI_SOURCES ${AAMP_CLI_SOURCES} drm/ave/StubsForAVEPlayer.cpp)
	#Linking QT libraries to libaamp will make local stubs obsolete for aamp-cli
	if(NOT CMAKE_QT5WEBKIT_JSBINDINGS AND CMAKE_WPEWEBKIT_JSBINDINGS)
		set(AAMP_CLI_EXTRA_DEFINES "${AAMP_CLI_EXTRA_DEFINES} -DAAMP_STUBS_FOR_JS")
	endif()
else()
    message("CMAKE_AVE_DRM not set, not adding stubs?")
endif()

if(CMAKE_USE_OPENCDM OR CMAKE_USE_OPENCDM_ADAPTER OR CMAKE_CDM_DRM)
    # Sources common to all forms of OpenCDM and CDM
    message("Using some form of OpenCDM or CDM")
    set(LIBAAMP_DRM_SOURCES drm/AampDRMSessionManager.cpp
                            drm/AampDrmSession.cpp
                            drm/aampdrmsessionfactory.cpp
                            drm/aampoutputprotection.cpp
                            drm/helper/AampDrmHelperFactory.cpp
                            drm/AampDRMutils.cpp
    )
    if(CMAKE_USE_THUNDER_OCDM_API_0_2)
        set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_THUNDER_OCDM_API_0_2")
    endif()
endif()

if(CMAKE_CDM_DRM)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_MPD_DRM=1")
endif()

if(CMAKE_USE_OPENCDM OR CMAKE_USE_OPENCDM_ADAPTER)
    message("OpenCDM support enabled")
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_OPENCDM -DAAMP_HLS_DRM=1")
    set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" drm/AampHlsDrmSessionManager.cpp
                                                     drm/AampHlsOcdmBridge.cpp
                                                     drm/processProtectionHls.cpp
    )
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DDRM_BUILD_PROFILE=DRM_BUILD_PROFILE_OEM -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0")

    if(CMAKE_IARM_MGR)
        set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lIARMBus -lds -ldshalcli")
    endif()

    # DRM Helpers
    if(CMAKE_USE_WIDEVINE)
        set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampWidevineDrmHelper.cpp)
    endif()

    if(CMAKE_USE_VGDRM)
        set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampVgdrmHelper.cpp)
        if(CMAKE_USE_ION_MEMORY)
            # Needed for ION Secure Memory
            find_path (REALTEK_INCDIR realtek)
            include_directories(${REALTEK_INCDIR}/realtek)
            include_directories(${REALTEK_INCDIR}/realtek/system/core/include)
            include_directories(${REALTEK_INCDIR}/realtek/bionic/libc/kernel/common)
            message("Looking in ${CMAKE_LIB_SYSROOT} for RTK ION libraries")
            find_library(REALTEK_ION_LIBRARY NAMES ion rtk_ion PATHS "${CMAKE_LIB_SYSROOT}/usr/lib" )
            if (NOT REALTEK_ION_LIBRARY)
                set(REALTEK_ION_LIBRARY "${CMAKE_LIB_SYSROOT}/usr/lib/realtek")
                message("Forced RTK ION libraries to ${REALTEK_ION_LIBRARY}")
            endif()
            link_directories(${REALTEK_ION_LIBRARY})
            set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lion -lrtk_ion")
            set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_ION_MEMORY")
            set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/AampIonMemorySystem.cpp)
        else()
            # Needed for Shared Memory
            set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lrt")
            set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SHARED_MEMORY")
            set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/AampSharedMemorySystem.cpp)
        endif()
    endif()

    if(CMAKE_USE_CLEARKEY)
        set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampClearKeyHelper.cpp)
    endif()

    if(CMAKE_USE_PLAYREADY)
        set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampPlayReadyHelper.cpp)
    endif()
else()
    message("No OpenCDM support enabled")
endif()

# Always include Adobe Access Helper
set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampAveDrmHelper.cpp)


if(CMAKE_USE_CLEARKEY)
    set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" drm/ClearKeyDrmSession.cpp)
    set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampClearKeyHelper.cpp)
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_CLEARKEY")
endif()

if(CMAKE_USE_OPENCDM_ADAPTER)
    message("Using OPEN CDM ADAPTER")
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_OPENCDM_ADAPTER -DUSE_OPENCDM")
    set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" drm/opencdmsessionadapter.cpp
                                                     drm/AampOcdmBasicSessionAdapter.cpp
                                                     drm/AampOcdmGstSessionAdapter.cpp
    )
    if(DEFINED SAGE_SVP)
            set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DCONTENT_4K_SUPPORTED")
    endif()
    set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -locdm")
    find_path (STAGING_INCDIR opencdm)
    include_directories(${STAGING_INCDIR}/opencdm)
    find_path (STAGING_INCDIR gstreamer-1.0)
    include_directories(${STAGING_INCDIR}/gstreamer-1.0)
elseif(CMAKE_USE_OPENCDM)
    message("Using OPEN CDM")
    set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" drm/opencdmsession.cpp)

    if (DEFINED SECAPI_ENGINE_BROADCOM_SAGE)
        set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECAPI_BRCMHW=1")
        if(DEFINED SAGE_SVP)
            set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SAGE_SVP -DCONTENT_4K_SUPPORTED")
            set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -locdm -lb_secbuf -lnexus -lbrcmsvpmeta -lsec_api")
        else()
            set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -locdm -lsec_api")
        endif()
    elseif(DEFINED SECAPI_ENGINE_BROADCOM_RAAGA)
        set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECAPI_BRCMHW=1")
        set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -locdm -lsec_api")
    else()
        set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -locdm -lsec_api_crypto")
    endif()
    find_path (STAGING_INCDIR opencdm)
    include_directories(${STAGING_INCDIR}/opencdm)
elseif(CMAKE_CDM_DRM)
    message("Not Using OPEN CDM but using CDM")
    if(CMAKE_USE_PLAYREADY)
        set(LIBAAMP_DRM_SOURCES "${LIBAAMP_DRM_SOURCES}" drm/playreadydrmsession.cpp )
        set(LIBAAMP_HELP_SOURCES "${LIBAAMP_HELP_SOURCES}" drm/helper/AampPlayReadyHelper.cpp)
        set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_PLAYREADY -DDRM_BUILD_PROFILE=DRM_BUILD_PROFILE_OEM -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0")
        set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lplayready")
        find_path (STAGING_INCDIR playready)
        find_path (STAGING_INCDIR playready/oem/common/inc)
        find_path (STAGING_INCDIR playready/oem/ansi/inc)
        include_directories(${STAGING_INCDIR}/playready)
        include_directories(${STAGING_INCDIR}/playready/oem/common/inc)
        include_directories(${STAGING_INCDIR}/playready/oem/ansi/inc)
    endif()
    if (DEFINED SECAPI_ENGINE_BROADCOM_SAGE)
        set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECAPI_BRCMHW=1")
        if(DEFINED SAGE_SVP)
            set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SAGE_SVP -DCONTENT_4K_SUPPORTED")
            set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lsec_api -lb_secbuf -lnexus -lbrcmsvpmeta")
        else()
            if (CMAKE_USE_PLAYREADY)
                set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lsec_api")
            endif(CMAKE_USE_PLAYREADY)
        endif()
    elseif(DEFINED SECAPI_ENGINE_BROADCOM_RAAGA)
        set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECAPI_BRCMHW=1")
        if (CMAKE_USE_PLAYREADY)
            set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lsec_api")
        endif()
    else()
        if (CMAKE_USE_PLAYREADY)
            set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lsec_api_crypto")
        endif()
    endif()

    if(CMAKE_IARM_MGR)
        set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lIARMBus -lds -ldshalcli")
    endif()
endif()

if (CMAKE_RDK_CC_ENABLED OR CMAKE_SUBTEC_CC_ENABLED)
    include_directories(closedcaptions)
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_CC_ENABLED")
	set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} closedcaptions/AampCCManager.cpp)
endif()

if (CMAKE_AAMP_RFC_REQUIRED)
        message("AAMP RFC set")
        set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_RFC_ENABLED")
        set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} AampRfc.cpp)
        set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -ltr181api")
endif()

if (CMAKE_RDK_CC_ENABLED)
	message("CMAKE_RDK_CC_ENABLED set")
	include_directories(${DIRECTFB_INCLUDE_DIRS})
	include_directories(${DIRECTFB_INCLUDE_DIRS}/fusion)
	include_directories(${DIRECTFB_INCLUDE_DIRS}/direct)
	include_directories(closedcaptions/rdk)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_RDK_CC_ENABLED")
	set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} closedcaptions/rdk/AampRDKCCManager.cpp)
	set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lrdkCCReader -lrdkCCManager -lrdkCCGfx -l${DIRECTFB_LIBRARIES}")
endif()

if (CMAKE_SUBTEC_CC_ENABLED)
    message("CMAKE_SUBTEC_CC_ENABLED set")

    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_SUBTEC_CC_ENABLED")

    set(LIBSUBTECCONNECTOR_DEFINES "${LIBAAMP_DEFINES}")
    set(LIBSUBTECCONNECTOR_DEPENDS pthread rdkCCReader)
    set(LIBSUBTECCONNECTOR_SOURCES  ${AAMP_SUBTEC_SOURCES}
        closedcaptions/subtec/SubtecConnector.cpp
        closedcaptions/subtec/CCDataController.cpp
        aamplogging.cpp)

    add_library(subtec_connector SHARED ${LIBSUBTECCONNECTOR_SOURCES})
    target_link_libraries(subtec_connector ${LIBSUBTECCONNECTOR_DEPENDS})
    install(TARGETS subtec_connector DESTINATION lib)

    link_directories("${CMAKE_BINARY_DIR}")

    set(LIBAAMP_SOURCES ${LIBAAMP_SOURCES} closedcaptions/subtec/AampSubtecCCManager.cpp)
    set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lsubtec_connector")
endif()


set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} ${SEC_CONTENT_METADATA_ENABLED}")

set(LIBAAMP_SOURCES "${LIBAAMP_SOURCES}" "${LIBAAMP_DRM_SOURCES}")

add_library(aamp SHARED ${LIBAAMP_SOURCES} ${LIBAAMP_HELP_SOURCES})
add_executable(aamp-cli ${AAMP_CLI_SOURCES})
add_executable(playbintest test/playbintest.cpp)
target_link_libraries(playbintest ${PLAYBINTEST_DEPENDS})

if(CMAKE_SOC_PLATFORM_INTEL)
	message("CMAKE_SOC_PLATFORM_INTEL set")
	set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lismd_core -lismd_vidpproc -lismd_vidsink_direct -lismd_vidrend -lismd_viddec  -lismd_core -lsysloghelper -lgdl")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DINTELCE=1 -DAAMP_STOP_SINK_ON_SEEK=1 -DUSE_SYSLOG_HELPER_PRINT=1 -DENABLE_AAMP_QTDEMUX_OVERRIDE=1")
endif()

if(CMAKE_SOC_PLATFORM_REALTEK)
        message("CMAKE_SOC_PLATFORM_REALTEK set")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DREALTEKCE=1 -DENABLE_AAMP_QTDEMUX_OVERRIDE=1 -DCONTENT_4K_SUPPORTED")
endif()

if(CMAKE_SOC_PLATFORM_RPI)
	message("CMAKE_SOC_PLATFORM_RPI set")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DRPI=1 -DENABLE_AAMP_QTDEMUX_OVERRIDE=1")
endif()

if(CMAKE_SYSTEMD_JOURNAL)
    message("CMAKE_SYSTEMD_JOURNAL set")
    set(LIBAAMP_DEPENDS "${LIBAAMP_DEPENDS} -lsystemd")
    set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SYSTEMD_JOURNAL_PRINT=1 -DSD_JOURNAL_SUPPRESS_LOCATION=1")
endif()

if(CMAKE_USE_SECCLIENT)
	message("CMAKE_USE_SECCLIENT set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECCLIENT")
endif()
if(CMAKE_USE_SECMANAGER)
	message("CMAKE_USE_SECMANAGER set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_SECMANAGER")
endif()

if(CMAKE_WPEWEBKIT_JSBINDINGS)
	message("CMAKE_WPEWEBKIT_JSBINDINGS set, creating jsbinding library")
	add_subdirectory(test)
	add_library(aampjsbindings SHARED jsbindings/jscontroller-jsbindings.cpp jsbindings/jsbindings.cpp jsbindings/jsutils.cpp jsbindings/jsmediaplayer.cpp jsbindings/jseventlistener.cpp jsbindings/jsevent.cpp)
    target_link_libraries(aampjsbindings aamp)

    if (CMAKE_RDK_CC_ENABLED OR CMAKE_SUBTEC_CC_ENABLED)
        set(LIBAAMPJSBINDINGS_DEFINES "${LIBAAMPJSBINDINGS_DEFINES} -DAAMP_CC_ENABLED")
    endif()

    set_target_properties(aampjsbindings PROPERTIES COMPILE_FLAGS "${LIBAAMPJSBINDINGS_DEFINES}")

	install(TARGETS aampjsbindings DESTINATION lib)
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_WPEWEBKIT_JSBINDINGS")
else()
    message("CMAKE_WPEWEBKIT_JSBINDINGS not set, not creating jsbinding library")
endif()

if(CMAKE_AUXILIARY_AUDIO_ENABLED)
	message("CMAKE_AUXILIARY_AUDIO_ENABLED set")
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DAAMP_AUXILIARY_AUDIO_ENABLED")
endif()

message("LIB_AAMP_DEPENDS is ${LIBAAMP_DEPENDS}")

target_link_libraries(aamp ${LIBAAMP_DEPENDS})

if (CMAKE_WPEFRAMEWORK_REQUIRED)
	# Remove compiler flag as it causes compilation issues with wpeframework headers
	STRING(REPLACE " -Werror=effc++" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
	set(LIBAAMP_DEFINES "${LIBAAMP_DEFINES} -DUSE_CPP_THUNDER_PLUGIN_ACCESS")
	target_link_libraries(aamp ${WPEFRAMEWORK_LIBRARIES})
endif()

target_link_libraries(aamp-cli aamp ${AAMP_CLI_LD_FLAGS})

set_target_properties(aamp PROPERTIES COMPILE_FLAGS "${LIBAAMP_DEFINES} ${OS_CXX_FLAGS}")
#aamp-cli is not an ideal standalone app. It uses private aamp instance for debugging purposes
set_target_properties(aamp-cli PROPERTIES COMPILE_FLAGS "${LIBAAMP_DEFINES} ${AAMP_CLI_EXTRA_DEFINES} ${OS_CXX_FLAGS}")
set_target_properties(aamp PROPERTIES PUBLIC_HEADER "main_aamp.h")
set_target_properties(aamp PROPERTIES PRIVATE_HEADER "priv_aamp.h")

install(TARGETS aamp-cli DESTINATION bin)
install(TARGETS playbintest DESTINATION bin)

install(TARGETS aamp DESTINATION lib PUBLIC_HEADER DESTINATION include PRIVATE_HEADER DESTINATION include)
install(FILES AampEvent.h AampConfig.h AampEventManager.h AampDefine.h AampEventListener.h drm/AampMemorySystem.h drm/AampDRMSessionManager.h AampDrmSystems.h AampMediaType.h
  drm/AampDrmData.h drm/AampDrmInfo.h drm/AampDrmMediaFormat.h AampDrmCallbacks.h AampLogManager.h 
  drm/AampDrmSession.h drm/ClearKeyDrmSession.h drm/AampDRMutils.h drm/aampdrmsessionfactory.h
  drm/helper/AampDrmHelper.h subtitle/vttCue.h AampUtils.h
  AampMemoryUtils.h AampProfiler.h AampConstants.h iso639map.h AampRfc.h AampGstUtils.h
  AampScheduler.h
  drm/AampSecManager.h
  DESTINATION include)

if (CMAKE_AAMP_RFC_REQUIRED)
install(FILES AampRfc.h DESTINATION include)
endif()

message("LIBAAMP_DEFINES is ${LIBAAMP_DEFINES}")
message("LIBAAMP_SOURCES is ${LIBAAMP_SOURCES}")
message("LIBAAMP_HELP_SOURCES is ${LIBAAMP_HELP_SOURCES}")

