#! /bin/sh

set -e

TOPDIR=$(cd $(dirname $0) && pwd -P)/../..
. ${TOPDIR}/all_features.sh

TOYWASM_EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS}" \
../build-toywasm-and-app.sh
