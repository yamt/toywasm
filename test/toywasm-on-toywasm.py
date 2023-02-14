#! /usr/bin/env python3

import argparse
import os
import os.path
import shlex
import sys
import subprocess

executable = os.getenv("TOYWASM", "./build.native/toywasm")
executable_wasm = os.getenv("TOYWASM_WASM", "./build.wasm/toywasm")

parser = argparse.ArgumentParser(allow_abbrev=False)
parser.add_argument("--wasi-dir", action="append", default=[])
parser.add_argument("--wasi-mapdir", action="append", default=[])
args, unknown = parser.parse_known_args()

sys_prefix = "@TOYWASM@"


def translate_path(cat, name):
    d = os.path.dirname(name)
    if d == "":
        d = "."
    wasm_path = os.path.join(sys_prefix, cat)
    options.append(f"--wasi-mapdir={wasm_path}::{d}")
    b = os.path.basename(name)
    return os.path.join(sys_prefix, cat, b)


options = []

for x in args.wasi_dir:
    options.append(f"--wasi-dir={x}")

for x in args.wasi_mapdir:
    # REVISIT: translate host path?
    g, h = x.split("::", maxsplit=2)
    options.append(f"--wasi-dir={h}")

# assume that the first argument which doesn't
# start with "--" is a wasm module.
user_args = sys.argv[1:]
for i in range(0, len(user_args)):
    if not user_args[i].startswith("--"):
        user_args[i] = translate_path("user-wasm", user_args[i])
        break

cmd = (
    shlex.split(executable) + ["--wasi"] + options + ["--", executable_wasm] + user_args
)
# print(cmd)
result = subprocess.run(cmd)
sys.exit(result.returncode)
