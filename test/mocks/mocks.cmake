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

cmake_minimum_required (VERSION 2.6)

#
# Copy a header file from a staging include directory.
#
# When compiling for Ubuntu or OS X, header files in a staging include
# directory, for example an RDK sysroot usr/include directory, may clash with
# host system files. This CMake function is used to copy header files from the
# staging include directory into a build include directory which can then be
# included.
#
function(copy_staging_header_file HEADER)
    if(CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin)
        # Copy the header file from the staging include root directory to where
        # it can be included without including system include files.
        find_path(STAGING_INCDIR ${HEADER})
        if(EXISTS ${STAGING_INCDIR}/${HEADER})
            file(COPY ${STAGING_INCDIR}/${HEADER}
                 DESTINATION ${CMAKE_BINARY_DIR}/staging)
        endif()
    endif()
endfunction()

# Include header files copied by copy_staging_header_file()
include_directories(${CMAKE_BINARY_DIR}/staging)
