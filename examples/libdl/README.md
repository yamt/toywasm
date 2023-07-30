# What's this

A simple dlopen/dlsym implementation backed by host functions.

# Prerequisites

Build toywasm with the following options to include the host functions.
```
TOYWASM_ENABLE_DYLD=ON
TOYWASM_ENABLE_DYLD_DLFCN=ON
```

Also, enable them with the `--dyld-dlfcn` runtime option.
```
toywasm --dyld --dyld-dlfcn
```
Or, via `dyld_options.enable_dlfcn` if you are using toywasm as a library.
