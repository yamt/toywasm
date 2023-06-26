#! /bin/sh

set -e

THIS_DIR=$(cd $(dirname $0) && pwd -P)

mkdir build.cross.${ARCH}
cd build.cross.${ARCH}
${THIS_DIR}/cmake-wrap.sh \
${EXTRA_CMAKE_OPTIONS} \
..
