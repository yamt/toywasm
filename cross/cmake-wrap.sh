#! /bin/sh

set -e

THIS_DIR=$(cd $(dirname $0) && pwd -P)

# tested ARCH: arm64 s390x armhf riscv64 i386

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

cmake \
-DCMAKE_TOOLCHAIN_FILE=${THIS_DIR}/cross.cmake \
-DTRIPLET=${TRIPLET} \
-DARCH=${ARCH} \
"$@"
