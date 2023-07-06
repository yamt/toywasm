# check_interrupt and restartable errors

## Overview

We have a few features which potentially block for long period.
(possibly forever.)
They include `wasm-threads` and `WASI`.

To make it possible to handle asynchronous requests like thread termination
while executing these long-blocking operations, they actually wake up
periodically and check asynchronous requests by calling the `check_interrupt`
function.

The `check_interrupt` function can return special error numbers
(eg. `ETOYWASMRESTART`, `ETOYWASMTRAP`, etc) to notify asynchronous requests.
It makes the thread rewind its stack up to the embedder (eg. the caller of
`instance_execute_func`) by returning the error as usual.

Some of these errors are restartable. (you can check it with the
`IS_RESTARTABLE` macro.) That is, after handling the request, an embedder
can choose to resume the execution.

An embedder usually handles these errors using the
`instance_execute_handle_restart` function or its variation.
It resumes the execution of the instance after performing the default
processing of the asyncronous requests unless it has unrestartable
conditions like a trap.

## long-blocking operations

We basically do never actually block for a long period.
Long-blocking operations are emulated with a loop similar
to the following pseudo code:

```c
    block_forever()
    {
        while (1) {
            ret = check_interrupt();
            if (ret != 0) {
                return ret;
            }
            sleep(short_interval);
        }
    }
```

### wasm-threads

#### `memory.atomic.wait32` and `memory.atomic.wait64` instructions

We use a shorter timeout to emulate the user-specified timeout.

### WASI

#### `fd_read` and similar functions

We use non-blocking I/O to emulate blocking I/O.

#### `poll_oneoff`

We use a shorter timeout to emulate the user-specified timeout.

## Users of this mechanism

This mechanism is used to implement:

* WASI `proc_exit` to terminate sibling threads

* preemption for the user scheduler (`TOYWASM_USE_USER_SCHED=ON`)

* shared memory reallocation (`TOYWASM_PREALLOC_SHARED_MEMORY=OFF`)

* embedder-driven interrupts (`exec_context::intrp`)
