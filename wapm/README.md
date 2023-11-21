## How to publish a new version to wapm

1. bump the version in wapm.toml

2. tag

3. build

```shell
% ./build.sh
```

4. test

```shell
% toywasm --wasi ./build/toywasm ......
```

5. publish

```shell
% wapm publish
```

## Running the binary from wapm

```
% wapm install yamt/toywasm
% wapm run toywasm
```

Or, on toywasm:
```
% WAPM_RUNTIME="toywasm --wasi" wapm run toywasm
```

Note: the stand-alone wapm has been discontinued.
I don't know what's an alternative these days.

Or, on [webassembly.sh](https://webassembly.sh/?run-command=toywasm).
