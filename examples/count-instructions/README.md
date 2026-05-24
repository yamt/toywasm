# What's this

A sample program to count instructions.

This program prints the largest functions in a given module.

The following is an example of the output:
```
1409 functions
219746 instructions
480386 bytes
2.1861 bytes / instructions
155.959 instructions / functions
340.941 bytes / functions

largest functions (in instructions):
    4439 : func 264 (printf_core)
    3862 : func 700 (fetch_process_next_insn_fd)
    2872 : func 517 (fetch_process_next_insn)
    2465 : func 301 (exec_expr_continue)
    2431 : func 279 (dlmalloc)

largest functions (in bytes):
    9777 : func 700 (fetch_process_next_insn_fd)
    8804 : func 264 (printf_core)
    7345 : func 517 (fetch_process_next_insn)
    5386 : func 279 (dlmalloc)
    5074 : func 301 (exec_expr_continue)
```
