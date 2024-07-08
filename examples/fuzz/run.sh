#! /bin/sh

export ASAN_OPTIONS=detect_leaks=1
mkdir -p corpus
./build/build-app/fuzz-instantiate -rss_limit_mb=4000 -malloc_limit_mb=3 corpus "$@"
