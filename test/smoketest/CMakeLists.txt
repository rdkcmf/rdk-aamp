include(ExternalProject)
include(GoogleTest)
set(CMAKE_CXX_STANDARD 14)
ExternalProject_Add(
  gtest
  URL https://github.com/google/googletest/archive/master.zip
  #GIT_REPOSITORY https://github.com/google/googletest.git
  #GIT_TAG release-1.8.1
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gtest
  INSTALL_COMMAND "")
ExternalProject_Get_Property(gtest source_dir binary_dir)
add_library(libgtest IMPORTED STATIC GLOBAL)
add_dependencies(libgtest gtest)
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
	set_target_properties(libgtest PROPERTIES
  		"IMPORTED_LOCATION" "${binary_dir}/lib/Debug/libgtestd.a"
  		"IMPORTED_LINK_INTERFACE_LIBRARIES" "${CMAKE_THREAD_LIBS_INIT}")
else()
	set_target_properties(libgtest PROPERTIES
  		"IMPORTED_LOCATION" "${binary_dir}/lib/libgtest.a"
  		"IMPORTED_LINK_INTERFACE_LIBRARIES" "${CMAKE_THREAD_LIBS_INIT}")
endif()
include_directories("${source_dir}/googletest/include" "${source_dir}" ../include)

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
  message("SIMULATOR_BUILD set")
  set(SMOKETEST_DEFINES "${SMOKETEST_DEFINES} -DSIMULATOR_BUILD=1")
endif()

set(HEADERS AampSmokeTestPlayer.h TuneSmokeTest.h)
set(SOURCES AampSmokeTestPlayer.cpp TuneSmokeTest.cpp gtest_main.cpp)
add_executable(aamp_smoketest ${HEADERS} ${SOURCES})
add_dependencies(aamp_smoketest aamp)
set_target_properties(aamp_smoketest PROPERTIES COMPILE_FLAGS "${SMOKETEST_DEFINES} ${LIBAAMP_DEFINES} ${AAMP_CLI_EXTRA_DEFINES} ${OS_CXX_FLAGS}")
target_link_libraries(aamp_smoketest aamp libgtest)
#gtest_discover_tests(aamp_gtest)
