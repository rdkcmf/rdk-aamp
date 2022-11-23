Pre-requisites to building the microtests:
------------------------------------------

AAMP installed using install-aamp.sh script which:
    - installs headers from dependent libraries
    - installs GoogleTest and GoogleMock
    
To build and run microtests using script:
-----------------------------------------

./run.sh

To build and run the microtests manually:
-----------------------------------------

mkdir build
cd build
cmake ../
make
ctest

Some examples of additional parameters that can be used with ctest
------------------------------------------------------------------

To output logging run ctest with verbose option:

ctest --verbose

To output logging when a test fails :

ctest --output-on-failure

Tests can be run in parallel using -j option, for example:

ctest -j 4

Specific tests can be run using ctest's regex selectors, see ctest --help. For example:

ctest -R PrivateInstance.*PositionAlready

