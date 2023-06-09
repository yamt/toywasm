#! /bin/sh

set -e

APP=$1
TGZ=$2
shift 2

DIR=$(mktemp -d)
gzip -cd ${TGZ} | (cd ${DIR} && pax -r)

cd examples/${APP}
BUILDDIR=$(mktemp -d)
cmake -B ${BUILDDIR} -DCMAKE_INSTALL_PREFIX=${DIR} .
cmake --build ${BUILDDIR}
${BUILDDIR}/${APP} "$@"
