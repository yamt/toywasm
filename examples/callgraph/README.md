# What's this

A sample program to investigate the function callgraph of the given module.

# Example

The graph of a typical WASI hello world program.

```c
#include <stdio.h>

int
main(int argc, char **argv)
{
        printf("hello\n");
}
```

```shell
% /opt/wasi-sdk-22.0/bin/clang a.c
% callgraph a.out | python3 cg_json2dot.py | dot -Tsvg -o hello.svg
```

![hello world call graph](./hello.svg)
