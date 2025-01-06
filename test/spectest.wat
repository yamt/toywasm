(;
spectest host module.
https://github.com/WebAssembly/spec/tree/main/interpreter#spectest-host-module

note that pure-wasm implementation of the "print" function is impossible.
this implementation should be enough to make start.wast pass though.

see also:
https://github.com/WebAssembly/spec/blob/d48af683f5e6d00c13f775ab07d29a15daf92203/test/harness/sync_index.js#L98
;)
(module
    (func (export "print"))
    (func (export "print_i32") (param i32))
    (func (export "print_i64") (param i64))
    (func (export "print_i32_f32") (param i32 f32))
    (func (export "print_f64_f64") (param f64 f64))
    (func (export "print_f32") (param f32))
    (func (export "print_f64") (param f64))
    (global (export "global_i32") i32 (i32.const 666))
    (global (export "global_i64") i64 (i64.const 666))
    (global (export "global_f32") f32 (f32.const 666.6))
    (global (export "global_f64") f64 (f64.const 666.6))
    (table (export "table") 10 20 funcref)
    (memory (export "memory") 1 2)
)
