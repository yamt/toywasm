(module
  (func $poll_oneoff (import "wasi_snapshot_preview1" "poll_oneoff") (param i32 i32 i32 i32) (result i32))
  (func $sleep_one_sec
    i32.const 0x118 ;; 0x100 + offsetof(subscription, timeout)
    i64.const 1_000_000_000 ;; 1s
    i64.store
    i32.const 0x100 ;; subscription
    i32.const 0x200 ;; event (out)
    i32.const 1   ;; nsubscriptions
    i32.const 0x300 ;; retp (out)
    call $poll_oneoff
    drop
  )
  (func (export "_start")
    loop
      call $sleep_one_sec
      br 0
    end
  )
  (memory (export "memory") 1)
)
