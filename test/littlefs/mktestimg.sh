#! /bin/sh

set -e

DIR=$(mktemp -d)

littlefs-python create \
--block-size=4096 \
--block-count=100 \
--image littlefs.img \
${DIR}

rm -r ${DIR}
