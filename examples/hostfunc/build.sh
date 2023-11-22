#! /bin/sh

set -e

cd wasm
./build.sh
cd ..

# use a debug build to enable assertions for now
EXTRA_CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=Debug" \
../build-toywasm-and-app.sh
