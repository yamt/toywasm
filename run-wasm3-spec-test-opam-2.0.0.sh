#! /bin/sh
./prepare-spec-test.sh
exec ./run-wasm3-spec-test.sh --spec-dir $(pwd -P)/.opam-2.0.0/test "$@"
