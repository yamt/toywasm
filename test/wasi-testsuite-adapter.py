#! /usr/bin/env python3

# an adapter for https://github.com/WebAssembly/wasi-testsuite

import argparse
import os
import shlex
import sys
import subprocess

# https://github.com/WebAssembly/wasi-testsuite/pull/46
executable = os.getenv("TEST_RUNTIME_EXE")
if executable is None:
    executable = os.getenv("TOYWASM", "toywasm")

parser = argparse.ArgumentParser()
parser.add_argument("--test-file")
parser.add_argument("--dir", action="append", default=[])
parser.add_argument("--env", action="append", default=[])
parser.add_argument("--arg", action="append", default=[])
parser.add_argument("--version", action="store_true")
args = parser.parse_args()

if args.version:
    # Note: wasi-testsuite expects runtime-name and version,
    # separated by a space.
    result = subprocess.run([executable, "--version"], capture_output=True)
    print(result.stdout.decode("utf-8").splitlines()[0])
    sys.exit(result.returncode)

options = []
for x in args.env:
    options.extend([f"--wasi-env={x}"])
for x in args.dir:
    options.extend([f"--wasi-dir={x}"])
result = subprocess.run(
    shlex.split(executable) + ["--wasi"] + options + ["--", args.test_file] + args.arg
)
sys.exit(result.returncode)
