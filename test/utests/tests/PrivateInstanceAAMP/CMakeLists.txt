# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2022 RDK Management
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

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME PrivateInstanceAAMPTests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/isobmff ${AAMP_ROOT}/drm ${AAMP_ROOT}/drm/helper ${AAMP_ROOT}/drm/ave ${AAMP_ROOT}/subtitle)
include_directories(${AAMP_ROOT}/subtec/libsubtec)
include_directories(${AAMP_ROOT}/subtec/subtecparser)

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include_directories(/usr/local/include)
    include_directories(/usr/local/include/libdash)
    set(OS_LD_FLAGS -L/usr/local/lib)

else()
    include_directories(${AAMP_ROOT}/Linux/include)
    include_directories(${AAMP_ROOT}/Linux/include/libdash)
    set(OS_LD_FLAGS -luuid)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LibXml2_INCLUDE_DIRS})
include_directories(${UTESTS_ROOT}/mocks)


set(TEST_SOURCES PauseAtTests.cpp
                 PrivateInstanceAAMPTests.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/priv_aamp.cpp)

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${AAMP_SOURCES})

# Can be removed once SESSION_STATS build issues resolved
set_target_properties(${EXEC_NAME} PROPERTIES COMPILE_FLAGS "-DSESSION_STATS")

target_link_libraries(${EXEC_NAME} fakes -lpthread ${GLIB_LDFLAGS} ${OS_LD_FLAGS} -lgmock -lgtest)

gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)