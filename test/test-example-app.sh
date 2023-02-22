#! /bin/sh

set -e

TGZ=$1
MODULE=$2

DIR=$(mktemp -d)
gzip -cd ${TGZ} | (cd ${DIR} && pax -r)

cd examples/app
BUILDDIR=$(mktemp -d)
cmake -B ${BUILDDIR} -DCMAKE_INSTALL_PREFIX=${DIR} .
cmake --build ${BUILDDIR}
${BUILDDIR}/app ${MODULE}
