#! /bin/sh

set -e

dpkg --add-architecture ${ARCH}
apt update
apt install -y crossbuild-essential-${ARCH} libcmocka-dev:${ARCH} wabt
