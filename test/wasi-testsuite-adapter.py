#! /usr/bin/env python3

# an adapter for https://github.com/WebAssembly/wasi-testsuite

import argparse
import os
import sys
import subprocess

executable = os.getenv("TOYWASM", "toywasm")

parser = argparse.ArgumentParser()
parser.add_argument("--test-file")
parser.add_argument("--env", action="append", default=[])
parser.add_argument("--arg", action="append", default=[])
parser.add_argument("--version", action="store_true")
args = parser.parse_args()

if args.version:
    # Note: "toywasm --version" is not in a format which
    # wasi-testsuite expects. use a dummy version for now.

    # result = subprocess.run([executable, "--version"])
    # sys.exit(result.returncode)
    print("toywasm 0.0")
    sys.exit(0)

env_options = []
for x in args.env:
    env_options.extend(["--wasi-env", x])
result = subprocess.run(
    [executable, "--wasi"] + env_options + ["--", args.test_file] + args.arg
)
sys.exit(result.returncode)
