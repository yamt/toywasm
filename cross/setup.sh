#! /bin/sh

set -e

THIS_DIR=$(cd $(dirname $0) && pwd -P)

export ARCH=${ARCH:-armhf}
echo "Setting up cross toolchain for ${ARCH}..."

${THIS_DIR}/setup-focal-proposed.sh
${THIS_DIR}/cross-setup.sh
${THIS_DIR}/setup-alternatives.sh
