Demonstrate a pure-wasm implementation of `wasi_snapshot_preview1:fd_read`.

With [component-model](https://github.com/WebAssembly/component-model)
I suppose something similar can be done with the following. (totally untested)
```
(component
  (core module $Pivot
    :
  )
  (core module $App
    :
  )
  (core module $Hook
    :
  )
  (core instance $pivot (instantiate $Pivot))
  (core instance $app (instantiate $App (with "wasi_snapshot_preview1" (instance $pivot))))
  (core instance $hook (instantiate $Hook (with "app" (instance $app)) (with "pivot" (instance $pivot))))
)
```

Inspired by:

* https://github.com/bytecodealliance/preview2-prototyping
* https://github.com/bytecodealliance/wit-bindgen/pull/421
* https://github.com/bytecodealliance/wit-bindgen/pull/338
