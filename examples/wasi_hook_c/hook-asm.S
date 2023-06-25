# I was originally trying to write this as a .wat file. However, I gave up
# because wat2wasm's relocational output support was too limited.
#
# On the other hand, this .S format seems to have its own limitations:
#
# - I couldn't really find any documentation about this .S file format,
#   especially the wasm version of it. Probably it's considered
#   an llvm-internal undocumented thing.
#
# - Also, this .S format doesn't support things which llvm doesn't
#   support. (like multi-memory proposal)
#   Maybe we have to write raw bytecode in hex?
#
# It's inconvenient to experiment as there seems to be no single tool which
# supports everything.

    .text

    .tabletype __indirect_function_table, funcref
    .globaltype __stack_pointer, i32

    .functype my_fd_read (i32,i32,i32,i32) -> (i32)
    .functype init_C () -> ()

    .functype pivot_set_func (i32,funcref) -> ()
    .import_module pivot_set_func,pivot
    .import_name pivot_set_func,set_func

    .globaltype sp, i32
    .globaltype mypage, i32
sp:
mypage:

    .globl _initialize
    .type _initialize,@function

    .globl set_func
    .type set_func,@function

_initialize:
    .functype _initialize () -> ()
    .export_name _initialize,_initialize

    # set up C context for this module.
    #
    # steal a page via memory.grow.
    # this requires the main module prepared for this.
    # cf. https://github.com/WebAssembly/wasi-libc/pull/377
    #
    # todo: tls

    .local i32

    i32.const 1
    memory.grow 0
    local.tee 0
    i32.const -1
    i32.eq
    if
      unreachable
    end_if
    local.get 0
    global.set mypage
    local.get 0
    i32.const 1
    i32.add
    i32.const 65536
    i32.mul
    global.set sp

    call init_C
    end_function

set_func:
    .functype set_func (i32, i32) -> ()
    local.get 0
    local.get 1
    table.get __indirect_function_table
    call pivot_set_func
    end_function
