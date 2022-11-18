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

include(GoogleTest)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME AampCliSetTests)

include_directories(${AAMP_ROOT} ${AAMP_ROOT}/test/aampcli ${AAMP_ROOT}/subtitle)

# Mac OS X
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    include_directories(/usr/local/include)
    set(OS_LD_FLAGS -L/usr/local/lib)
else()
    include_directories(${AAMP_ROOT}/Linux/include)
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${UTESTS_ROOT}/mocks)

set(TEST_SOURCES ExecuteTests.cpp
                 
                 AampCliSetTests.cpp)

set(AAMP_SOURCES ${AAMP_ROOT}/test/aampcli/AampcliSet.cpp)

add_executable(${EXEC_NAME}
               ${TEST_SOURCES}
               ${AAMP_SOURCES})

target_link_libraries(${EXEC_NAME} fakes -lpthread ${GLIB_LDFLAGS} ${OS_LD_FLAGS} -lgmock -lgtest)

gtest_discover_tests(${EXEC_NAME} TEST_PREFIX ${EXEC_NAME}:)
