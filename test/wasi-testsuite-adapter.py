#! /usr/bin/env python3

# an adapter for https://github.com/WebAssembly/wasi-testsuite

import argparse
import os
import shlex
import sys
import signal
import subprocess


fstype = os.getenv("TOYWASM_WASI_FILESYSTEM_TYPE", "host")
if fstype == "littlefs":
    import pathlib
    from littlefs import LittleFS
    import tempfile


def handler(a, b):
    sys.exit(1)  # this terminates subrpocess.run


signal.signal(signal.SIGTERM, handler)


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
images = []
for x in args.env:
    options.extend([f"--wasi-env={x}"])
for x in args.dir:
    if fstype == "littlefs":
        fs = LittleFS(block_size=4096, block_count=1000)
        source_dir = pathlib.Path(x)
        fs.mkdir(source_dir.as_posix())
        for p in source_dir.rglob("*"):
           if p.is_dir():
               fs.mkdir(p.as_posix())
           else:
               with fs.open(p.as_posix(), "wb") as fp:
                   fp.write(p.read_bytes())
        image = tempfile.NamedTemporaryFile()
        image.write(fs.context.buffer)
        images.append(image)
        options.extend([f"--wasi-littlefs-dir={image.name}::{x}"])
    else:
        options.extend([f"--wasi-dir={x}"])
result = subprocess.run(
    shlex.split(executable) + ["--wasi"] + options + ["--", args.test_file] + args.arg
)
sys.exit(result.returncode)
