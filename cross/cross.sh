#! /bin/sh

set -e

# tested ARCH: arm64 s390x armhf

ARCH=${ARCH:-arm64}
case ${ARCH} in
arm64)
    TRIPLET=${TRIPLET:-aarch64-linux-gnu}
    ;;
armhf)
    TRIPLET=${TRIPLET:-arm-linux-gnueabihf}
    ;;
*)
    TRIPLET=${TRIPLET:-${ARCH}-linux-gnu}
    ;;
esac

dpkg --add-architecture ${ARCH}
apt update
apt install -y crossbuild-essential-${ARCH} libcmocka-dev:${ARCH} wabt
mkdir build.cross.${ARCH}
cd build.cross.${ARCH}
cmake \
-DCMAKE_TOOLCHAIN_FILE=../cross/cross.cmake \
-DTRIPLET=${TRIPLET} \
..
make
