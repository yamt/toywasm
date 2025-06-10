#! /bin/sh

set -e

APP=$1
TGZ=$2
BUILDDIR=$3

DIR=$(mktemp -d)
gzip -cd ${TGZ} | (cd ${DIR} && pax -r)

cd examples/${APP}
cmake -B ${BUILDDIR} -DCMAKE_INSTALL_PREFIX=${DIR} .
cmake --build ${BUILDDIR}
