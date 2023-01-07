cmake_minimum_required (VERSION 2.6)
find_package(GTest 1.12.1 REQUIRED)
pkg_check_modules(GOOGLETEST REQUIRED GTest)
include_directories(${GOOGLETEST_INCLUDE_DIRS})

if (CMAKE_PLATFORM_UBUNTU OR CMAKE_SYSTEM_NAME STREQUAL Darwin )
  message("SIMULATOR_BUILD set")
  set(SMOKETEST_DEFINES "${SMOKETEST_DEFINES} -DSIMULATOR_BUILD=1")
endif()

set(HEADERS AampSmokeTestPlayer.h TuneSmokeTest.h)
set(SOURCES AampSmokeTestPlayer.cpp TuneSmokeTest.cpp gtest_main.cpp)
add_executable(aamp_smoketest ${HEADERS} ${SOURCES})
add_dependencies(aamp_smoketest aamp)
set_target_properties(aamp_smoketest PROPERTIES COMPILE_FLAGS "${SMOKETEST_DEFINES} ${LIBAAMP_DEFINES} ${AAMP_CLI_EXTRA_DEFINES} ${OS_CXX_FLAGS}")
target_link_libraries(aamp_smoketest aamp ${GOOGLETEST_LIBRARIES})

install (TARGETS aamp_smoketest DESTINATION lib)
#gtest_discover_tests(aamp_smoketest) 
