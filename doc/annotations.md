# Annotations

## Overview

While toywasm maps wasm modules read-only and never modifies them in-place,
it still generates a few types of offline annotations on the bytecode
to avoid being too slow. While they are smaller than a full translation,
you might consider them a kind of translation.

## Jump table

This is to speed up branching. Wasm branch instruction don't have
a cheap way to know the destination address to jump to. Without this
table, whenever we execute a forward-branching instruction, we need
to parse every instructions the branch would skip over.

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
of the above mentioned cli options.
The fixed-size here is 128-bit for the `v128` type if toywasm is
built with SIMD enabled. (`-D TOYWASM_ENABLE_WASM_SIMD=ON`, which is
the default) Othewise, it's 64-bit.
While fixed-sized values incurs up to 4x memory overhead (eg. by using
128 bits to store an i32 value) for wasm operand stack, wasm locals,
and wasm tables, it's probably more cpu-efficient especially on a
64-bit host. Depending on situations, it can be memory-efficient as
well because it doesn't involve the static overhead of this table.

## Type annotations for value-polymorphic instructions

Some wasm instructions like `drop` works on a value of any types and
there is no cheap way to know the type at runtime.
While validating the bytecode, toywasm annotates these instructions
with the sizes of the values so that the necessary infomation is
available when executing the code later. While it's theoretically
possible to calculate them at the execution time, it would be
something like repeating the validation step. An alternative is
to annotate each values on the stack. But it's likely even more
expensive.

This annotation is unconditionally enabled if and only if toywasm is
built with variable-sized values, which is the default.
(`-D TOYWASM_USE_SMALL_CELLS=ON`)
