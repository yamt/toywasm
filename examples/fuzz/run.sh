#! /bin/sh

export ASAN_OPTIONS=detect_leaks=1
mkdir -p corpus
./build/build-app/fuzz-instantiate -rss_limit_mb=1000000 -malloc_limit_mb=100000 corpus "$@"
