(module
  (func $proc_exit (import "wasi_snapshot_preview1" "proc_exit") (param i32))
  (func $poll_oneoff (import "wasi_snapshot_preview1" "poll_oneoff") (param i32 i32 i32 i32) (result i32))
  (func $f (result i32)
    ;; clock_realtime, !abstime (zeros)
    i32.const 824 ;; 800 + offsetof(subscription, timeout)
    i64.const 1_000_000_000 ;; 1s
    i64.store
    i32.const 800 ;; subscription
    i32.const 400 ;; event (out)
    i32.const 1   ;; nsubscriptions
    i32.const 200 ;; retp (out)
    return_call $poll_oneoff
  )
  (func $exit (param i32)
    local.get 0
    return_call $proc_exit
  )
  (func (export "_start")
    call $f
    call $exit
  )
  (memory (export "memory") 1)
)
