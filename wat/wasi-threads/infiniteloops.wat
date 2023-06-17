(module
  (memory (export "memory") (import "env" "memory") 1 1 shared)
  (func $thread_spawn (import "wasi" "thread-spawn") (param i32) (result i32))
  (func (export "wasi_thread_start") (param $tid i32) (param $user_arg i32)
    loop
      br 0
    end
  )
  (func $spawn (param i32)
    ;; spawn a thread
    local.get 0
    call $thread_spawn
    ;; check error
    i32.const 0
    i32.le_s
    if
      unreachable
    end
  )
  (func $test (export "test") (param $num i32) (local $tmp i32)
    i32.const 0
    local.get $num
    i32.store
    ;; spawn threads
    local.get $num
    loop (param i32)
      local.tee $tmp
      local.get $tmp
      call $spawn
      i32.const 1
      i32.sub
      local.tee $tmp
      local.get $tmp
      br_if 0
      drop
    end
    loop
      br 0
    end
  )
  (func (export "_start")
    i32.const 8
    call $test
  )
)
