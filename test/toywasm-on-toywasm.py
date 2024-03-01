#! /usr/bin/env python3

# TODO
# - handle HOST_DIR::GUEST_DIR case
# - handle --wasi-littlefs-dir

import argparse
import os
import os.path
import shlex
import sys
import subprocess

ourdir = os.path.dirname(os.path.abspath(__file__))
executable = os.getenv(
    "TOYWASM_NATIVE", os.path.join(ourdir, "../build.native/toywasm")
)
executable_wasm = os.getenv(
    "TOYWASM_WASM", os.path.join(ourdir, "../build.wasm/toywasm")
)

parser = argparse.ArgumentParser(allow_abbrev=False)
parser.add_argument("--wasi-dir", action="append", default=[])
args, unknown = parser.parse_known_args()

sys_prefix = "@TOYWASM@"
debug = False


def translate_path(cat, name):
    d = os.path.dirname(name)
    if d == "":
        d = "."
    wasm_path = os.path.join(sys_prefix, cat)
    options.append(f"--wasi-dir={d}::{wasm_path}")
    b = os.path.basename(name)
    return os.path.join(sys_prefix, cat, b)


options = []

for x in args.wasi_dir:
    options.append(f"--wasi-dir={x}")

# assume that the first argument which doesn't start with "--" is
# a wasm module.
# also, translate --load argument.
# note: this logic recognizes only a subset of toywasm cli options.
# you can use "--invoke=foo" instead of "--invoke foo" to avoid
# misinterpretations.
user_args = sys.argv[1:]
if debug:
    print(f"Original: {user_args}")
load_arg = False
options_done = False
for i in range(0, len(user_args)):
    if load_arg:
        user_args[i] = translate_path(f"load-wasm-{i}", user_args[i])
        load_arg = False
        continue
    if not options_done:
        if user_args[i] == "--":
            options_done = True
            continue
        if user_args[i] == "--load":
            load_arg = True
            continue
        if user_args[i].startswith("--load="):
            _, wasm = user_args[i].split("=", 1)
            translated = translate_path(f"load-wasm-{i}", wasm)
            user_args[i] = f"--load={translated}"
            continue
    if options_done or not user_args[i].startswith("--"):
        user_args[i] = translate_path("user-wasm", user_args[i])
        break
if debug:
    print(f"Translated: {user_args}")
    print(f"Host toywasm options: {options}")

cmd = (
    shlex.split(executable) + ["--wasi"] + options + ["--", executable_wasm] + user_args
)
# print(cmd)
result = subprocess.run(cmd)
sys.exit(result.returncode)
