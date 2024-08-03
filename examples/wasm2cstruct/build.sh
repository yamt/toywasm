#! /bin/sh

set -e

TOPDIR=$(cd $(dirname $0) && pwd -P)/../..
. ${TOPDIR}/all_features.sh

# use a debug build to enable assertions for now
TOYWASM_EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DCMAKE_BUILD_TYPE=Debug" \
APP_EXTRA_CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=Debug" \
../build-toywasm-and-app.sh
