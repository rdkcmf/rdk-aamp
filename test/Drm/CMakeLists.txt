cmake_minimum_required(VERSION 2.6)

find_package(PkgConfig REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

pkg_search_module(GLIB REQUIRED glib-2.0)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-app-1.0)
pkg_check_modules(CURL REQUIRED libcurl)

project(DrmTest)
set(AAMP_ROOT "../../")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_GST1 -ggdb")
set(CPPUTEST_LDFLAGS CppUTest CppUTestExt)
set(EXEC_NAME drmTests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/subtitle ${AAMP_ROOT}/metrics)
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})

find_path (STAGING_INCDIR opencdm)
include_directories(${STAGING_INCDIR}/opencdm)
include_directories(mocks)

set(TEST_SOURCES drmTests.cpp
                 drmUtilsTest.cpp)

set(MOCK_SOURCES mocks/aampMocks.cpp
                 mocks/curlMocks.c
                 mocks/opencdmMocks.c
                 mocks/gstMocks.c
                 mocks/pthreadMocks.c
                 mocks/openSslMocks.c)

set(AAMP_SOURCES ${AAMP_ROOT}/_base64.cpp
                 ${AAMP_ROOT}/AampJsonObject.cpp
                 ${AAMP_ROOT}/drm/AampDRMutils.cpp
                 ${AAMP_ROOT}/drm/aampoutputprotection.cpp
                 ${AAMP_ROOT}/drm/aampdrmsessionfactory.cpp
                 ${AAMP_ROOT}/drm/helper/AampDrmHelper.cpp
                 ${AAMP_ROOT}/drm/helper/AampDrmHelperFactory.cpp
                 ${AAMP_ROOT}/drm/AampDRMSessionManager.cpp
                 ${AAMP_ROOT}/drm/AampDrmSession.cpp)

if(CMAKE_ENABLE_LOGGING)
    add_definitions(-DENABLE_LOGGING)
endif()

if(CMAKE_USE_OPENCDM_ADAPTER)
    add_definitions(-DUSE_OPENCDM -DUSE_OPENCDM_ADAPTER)
    set(TEST_SOURCES ${TEST_SOURCES} drmHelperTest.cpp drmSessionTest.cpp)
    set(AAMP_SOURCES ${AAMP_SOURCES} ${AAMP_ROOT}/drm/opencdmsessionadapter.cpp
                                     ${AAMP_ROOT}/drm/opencdmsessionadapter.cpp
                                     ${AAMP_ROOT}/drm/AampOcdmBasicSessionAdapter.cpp
                                     ${AAMP_ROOT}/drm/AampOcdmGstSessionAdapter.cpp
                                     ${AAMP_ROOT}/drm/AampHlsOcdmBridge.cpp
                                     ${AAMP_ROOT}/drm/AampSharedMemorySystem.cpp
                                     ${AAMP_ROOT}/drm/helper/AampVgdrmHelper.cpp
                                     ${AAMP_ROOT}/drm/helper/AampClearKeyHelper.cpp
                                     ${AAMP_ROOT}/drm/helper/AampWidevineDrmHelper.cpp
                                     ${AAMP_ROOT}/drm/helper/AampPlayReadyHelper.cpp)
    add_definitions(-DUSE_SHARED_MEMORY)
else()
    add_definitions(-DUSE_CLEARKEY)
    set(AAMP_SOURCES ${AAMP_SOURCES} ${AAMP_ROOT}/drm/ClearKeyDrmSession.cpp
                                     ${AAMP_ROOT}/drm/helper/AampClearKeyHelper.cpp)
    set(TEST_SOURCES ${TEST_SOURCES} legacyDrmSessionTest.cpp)
endif()

if(CMAKE_USE_MPD_DRM)
    add_definitions(-DAAMP_MPD_DRM)
endif()

if(CMAKE_USE_SECCLIENT)
    add_definitions(-DUSE_SECCLIENT)
    set(MOCK_SOURCES ${MOCK_SOURCES} mocks/secclientMocks.c)
endif()

add_executable(${EXEC_NAME}
               drmTestUtils.cpp
               ${TEST_SOURCES}
               ${MOCK_SOURCES}
               ${AAMP_SOURCES})

target_link_libraries(${EXEC_NAME} -lcjson -luuid -lrt -lpthread ${GLIB_LDFLAGS} ${CPPUTEST_LDFLAGS})

add_custom_target(run_tests COMMAND ./${EXEC_NAME} DEPENDS ${EXEC_NAME})
