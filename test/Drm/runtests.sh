set -e

run_secclient_tests=false
cmake_opts=""

while getopts "vs" opt; do
    echo $opt
    case "$opt" in
    v)
        cmake_opts="$cmake_opts -DCMAKE_ENABLE_LOGGING=TRUE"
        ;;
    s)
        run_secclient_tests=true
        ;;
    *)
        exit
        ;;
    esac
done

mkdir -p build && cd build

echo
echo "------ Building legacy tests ------"
mkdir -p legacy && pushd legacy
cmake ../../ $cmake_opts -DCMAKE_CDM_DRM=TRUE && make
popd

echo
echo "------ Building OCDM tests ------"
mkdir -p ocdm && pushd ocdm
cmake ../../ $cmake_opts -DCMAKE_USE_OPENCDM_ADAPTER=TRUE && make
popd

if $run_secclient_tests; then
    echo
    echo "------ Building secure client tests ------"
    mkdir -p secclient && pushd secclient
    cmake ../../ $cmake_opts -DCMAKE_USE_OPENCDM_ADAPTER=TRUE -DCMAKE_USE_SECCLIENT=TRUE && make
    popd
fi

echo
echo "------ Running legacy tests ------"
pushd legacy && make run_tests && popd

echo
echo "------ Running OCDM tests ------"
pushd ocdm && make run_tests && popd

if $run_secclient_tests; then
    echo
    echo "------ Running sec client tests ------"
    pushd secclient && make run_tests && popd
else
    echo
    echo "------ WARNING! Secure client tests were not run ------"
    echo "Ensure the headers and installed and use -s to enable"
fi
