# AAMP Microtests

A test infrastructure using GoogleTest C++ testing and mocking framework, to verify the behaviour of individual AAMP objects.

CTest is a testing tool that is part of CMake, and is used to automatically execute all the tests, and provides a report of the tests run, whether they passed/failed and time taken. It can be configured to run tests in parallel, output logging on failure, run specific tests etc.

These tests should be extended on addition of any new functionality.

These tests should be run on any change to:
 - Test any new functionality
 - Check for any regression caused by the change
 - Check the build has not been broken due to an API change

NOTE: Writing microtests is a really useful tool in improving code quality but if they are implemented incorrectly they can have a detrimental impact on build times and fail to find the errors they are meant to highlight. If you are new to microtests we strongly recommend you read the following to understand how to write them well:

 - **See [GoogleTest User's Guide](https://google.github.io/googletest/)**
 - **See [Testing With CMake and CTest](https://cmake.org/cmake/help/book/mastering-cmake/chapter/Testing%20With%20CMake%20and%20CTest.html)**

## Pre-requisites to building:

AAMP installed using install-aamp.sh script which:
 - installs headers from dependent libraries
 - installs GoogleTest and GoogleMock

## Build and run microtests using script:

From the *utests* folder run:

./run.sh

## To build and run the microtests manually:

From the *utests* folder run:

mkdir build
cd build
cmake ../
make
ctest

## Some examples of additional parameters that can be used with ctest

To output logging run ctest with verbose option:

ctest --verbose

To output logging when a test fails :

ctest --output-on-failure

Tests can be run in parallel using -j option, for example:

ctest -j 4

Specific tests can be run using ctest's regex selectors, see ctest --help. For example:

ctest -R PrivateInstance.*PositionAlready

## Directory Structure

### fakes

A CMake library containing fake/stub implementations of class methods, to allow compiling of class under test in isolation; these fakes are common to all tests.

Implementation can be extended to call a mock instance, to allow testing of expectations. For example, see FakePrivateInstanceAAMP.cpp where some methods being used by existing tests have been extended to call a mock of PrivateInstanceAAMP if the mock has been constructed.

The files in here will likely need to be updated for any API changes/additions made to AAMP modules, otherwise unresolved symbol errors are likely to be seen.

### mocks

A directory containing Google mocks; these mocks are common to all tests.

See [gMock for Dummies](https://google.github.io/googletest/gmock_for_dummies.html)

### tests

A directory containing the tests for each of AAMP's modules contained within their own sub-directory.
The CMakeLists.txt file adds all the modules' subdirectories.

See [CMake Modules - GoogleTest](https://cmake.org/cmake/help/latest/module/GoogleTest.html)

### *Module* test folder

One or more Googletest executables generated from CMake.

The CMakeLists.txt contains the instructions for creating the module's target. e.g.
- Necessary include paths for that module
- The module source file
- The google test file(s)
- The directive, gtest_discover_tests, to discover google tests from the test executable 

General guidance:
 - A test class for each area of functionality of the module under test.
 - Use of mocks, and testing of expectations, for external calls made by the module
 - Use of microtests should not significantly impact build times, and are intended to be run on every change, as such they should be implemented to run as quickly as possible (all tests to run in seconds rather than minutes); avoiding sleeps and other timing delays.

It may be desired to use the real implementation of an external class, rather than the mock; which can be done in CMakeLists.txt. Also if some tests are best implemented using a mock, and others using the real implementation then it should be possible to create multiple executables configured as such.

For guidance on creating GoogleTest please see [GoogleTest User's Guide](https://google.github.io/googletest/).

