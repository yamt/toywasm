# Annotations

## Overview

While toywasm maps wasm modules read-only and never modifies them in-place,
it still generates a few types of offline annotations on the bytecode
to avoid being too slow. While they are smaller than a full translation,
you might consider them a kind of translation.

## Jump table

This is to speed up forward branching. Wasm branch instruction don't
have a cheap way to know the destination address to jump to. Without
this table, whenever we execute a forward-branching instruction, we
need to parse every instructions the branch would skip over.

This is optional and can be disabled by the `--disable-jump-table`
runtime option.

## Local offset tables

This is to speed up access to locals (E.g. `local.get`) in case
toywasm is built with variable-sized values, which is the default.
(`-D TOYWASM_USE_SMALL_CELLS=ON`)
Without this table, an access to a local is O(x) where x is
the number of locals in the function, including function arguments.

You can disable them by `--disable-localtype-cellidx`
and the `--disable-resulttype-cellidx` runtime options.

When toywasm is built to use fixed-sized values,
(`-D TOYWASM_USE_SMALL_CELLS=OFF`) an access to a local is naturally
O(1).  In that case, this table is not necessary or used, regardless
of the above mentioned options.
The fixed-size here is the size of the largest value enabled by the
build-time configuratons. That is, 128-bit for the `v128` type if
toywasm is built with SIMD enabled. (`-D TOYWASM_ENABLE_WASM_SIMD=ON`,
which is the default) Othewise, it's 64-bit.
Fixed-sized values incurs up to 4x memory overhead (eg. by using
128 bits to store an i32 value) for wasm operand stack, wasm locals,
and wasm tables. While it might sound very inefficient, it's likely
more cpu-efficient for typical workloads, especially on a 64-bit host.
Depending on situations, it can be even memory-efficient as well because
it doesn't involve the static overhead of this table.

## Type annotations for value-polymorphic instructions

Some wasm instructions like `drop` works on a value of any types.
In the spec, it's called value-polymorphic. Unfortunately, for toywasm,
there is no cheap way to know the type when executing the instruction.

While validating the bytecode, toywasm annotates these instructions
with the sizes of the values so that the necessary infomation is
available when executing the code later. While it's theoretically
possible to calculate them at the execution time, it would be
something like repeating the validation step. An alternative is
to annotate each values on the stack. But it's likely even more
expensive. (It can be useful for other things like GC though.)

This annotation is unconditionally enabled if and only if toywasm is
built with variable-sized values, which is the default.
(`-D TOYWASM_USE_SMALL_CELLS=ON`)

## Overhead of the annotations

The memory consumption for the above mentioned annotations
depends on the wasm bytecode to annotate.
The following is a few examples taken with wasm modules I happened
to have.

### toywasm (from toywasm-v28.0.0-wasm32-wasi.tgz)

```
 wasm instructions to annotate       477455 bytes
           jump table overhead        87620 bytes
      type annotation overhead        22168 bytes
  local type cell idx overhead        23798 bytes
 result type cell idx overhead         1042 bytes
```

### spidermonkey

```
 wasm instructions to annotate      4143921 bytes
           jump table overhead      1096632 bytes
      type annotation overhead       142304 bytes
  local type cell idx overhead       109880 bytes
 result type cell idx overhead         2448 bytes
```

### ffmpeg

```
 wasm instructions to annotate     15442793 bytes
           jump table overhead      2401600 bytes
      type annotation overhead       335344 bytes
  local type cell idx overhead       388730 bytes
 result type cell idx overhead         5882 bytes
```
