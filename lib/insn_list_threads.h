/*
 * instructions from threads proposal.
 * https://github.com/WebAssembly/threads/blob/main/proposals/threads/Overview.md
 *
 * all instructions are prefixed by 0xfe.
 */

/* clang-format off */

INSTRUCTION(0x00, "memory.atomic.notify", memory_atomic_notify, 0)
INSTRUCTION(0x01, "memory.atomic.wait32", memory_atomic_wait32, 0)
INSTRUCTION(0x02, "memory.atomic.wait64", memory_atomic_wait64, 0)

INSTRUCTION(0x03, "atomic_fence", atomic_fence, 0)

/* load */

INSTRUCTION(0x10, "i32.atomic.load", i32_atomic_load, 0)
INSTRUCTION(0x11, "i64.atomic.load", i64_atomic_load, 0)

INSTRUCTION(0x12, "i32.atomic.load8_u", i32_atomic_load8_u, 0)
INSTRUCTION(0x13, "i32.atomic.load16_u", i32_atomic_load16_u, 0)

INSTRUCTION(0x14, "i64.atomic.load8_u", i64_atomic_load8_u, 0)
INSTRUCTION(0x15, "i64.atomic.load16_u", i64_atomic_load16_u, 0)
INSTRUCTION(0x16, "i64.atomic.load32_u", i64_atomic_load32_u, 0)

/* store */

INSTRUCTION(0x17, "i32.atomic.store", i32_atomic_store, 0)
INSTRUCTION(0x18, "i64.atomic.store", i64_atomic_store, 0)

INSTRUCTION(0x19, "i32.atomic.store8_u", i32_atomic_store8_u, 0)
INSTRUCTION(0x1a, "i32.atomic.store16_u", i32_atomic_store16_u, 0)

INSTRUCTION(0x1b, "i64.atomic.store8_u", i64_atomic_store8_u, 0)
INSTRUCTION(0x1c, "i64.atomic.store16_u", i64_atomic_store16_u, 0)
INSTRUCTION(0x1d, "i64.atomic.store32_u", i64_atomic_store32_u, 0)

/* add */

INSTRUCTION(0x1e, "i32.atomic.rmw.add", i32_atomic_rmw_add, 0)
INSTRUCTION(0x1f, "i64.atomic.rmw.add", i64_atomic_rmw_add, 0)

INSTRUCTION(0x20, "i32.atomic.rmw8.add_u", i32_atomic_rmw8_add_u, 0)
INSTRUCTION(0x21, "i32.atomic.rmw16.add_u", i32_atomic_rmw16_add_u, 0)

INSTRUCTION(0x22, "i64.atomic.rmw8.add_u", i64_atomic_rmw8_add_u, 0)
INSTRUCTION(0x23, "i64.atomic.rmw16.add_u", i64_atomic_rmw16_add_u, 0)
INSTRUCTION(0x24, "i64.atomic.rmw32.add_u", i64_atomic_rmw32_add_u, 0)

/* sub */

INSTRUCTION(0x25, "i32.atomic.rmw.sub", i32_atomic_rmw_sub, 0)
INSTRUCTION(0x26, "i64.atomic.rmw.sub", i64_atomic_rmw_sub, 0)

INSTRUCTION(0x27, "i32.atomic.rmw8.sub_u", i32_atomic_rmw8_sub_u, 0)
INSTRUCTION(0x28, "i32.atomic.rmw16.sub_u", i32_atomic_rmw16_sub_u, 0)

INSTRUCTION(0x29, "i64.atomic.rmw8.sub_u", i64_atomic_rmw8_sub_u, 0)
INSTRUCTION(0x2a, "i64.atomic.rmw16.sub_u", i64_atomic_rmw16_sub_u, 0)
INSTRUCTION(0x2b, "i64.atomic.rmw32.sub_u", i64_atomic_rmw32_sub_u, 0)

/* and */

INSTRUCTION(0x2c, "i32.atomic.rmw.and", i32_atomic_rmw_and, 0)
INSTRUCTION(0x2d, "i64.atomic.rmw.and", i64_atomic_rmw_and, 0)

INSTRUCTION(0x2e, "i32.atomic.rmw8.and_u", i32_atomic_rmw8_and_u, 0)
INSTRUCTION(0x2f, "i32.atomic.rmw16.and_u", i32_atomic_rmw16_and_u, 0)

INSTRUCTION(0x30, "i64.atomic.rmw8.and_u", i64_atomic_rmw8_and_u, 0)
INSTRUCTION(0x31, "i64.atomic.rmw16.and_u", i64_atomic_rmw16_and_u, 0)
INSTRUCTION(0x32, "i64.atomic.rmw32.and_u", i64_atomic_rmw32_and_u, 0)

/* or */

INSTRUCTION(0x33, "i32.atomic.rmw.or", i32_atomic_rmw_or, 0)
INSTRUCTION(0x34, "i64.atomic.rmw.or", i64_atomic_rmw_or, 0)

INSTRUCTION(0x35, "i32.atomic.rmw8.or_u", i32_atomic_rmw8_or_u, 0)
INSTRUCTION(0x36, "i32.atomic.rmw16.or_u", i32_atomic_rmw16_or_u, 0)

INSTRUCTION(0x37, "i64.atomic.rmw8.or_u", i64_atomic_rmw8_or_u, 0)
INSTRUCTION(0x38, "i64.atomic.rmw16.or_u", i64_atomic_rmw16_or_u, 0)
INSTRUCTION(0x39, "i64.atomic.rmw32.or_u", i64_atomic_rmw32_or_u, 0)

/* xor */

INSTRUCTION(0x3a, "i32.atomic.rmw.xor", i32_atomic_rmw_xor, 0)
INSTRUCTION(0x3b, "i64.atomic.rmw.xor", i64_atomic_rmw_xor, 0)

INSTRUCTION(0x3c, "i32.atomic.rmw8.xor_u", i32_atomic_rmw8_xor_u, 0)
INSTRUCTION(0x3d, "i32.atomic.rmw16.xor_u", i32_atomic_rmw16_xor_u, 0)

INSTRUCTION(0x3e, "i64.atomic.rmw8.xor_u", i64_atomic_rmw8_xor_u, 0)
INSTRUCTION(0x3f, "i64.atomic.rmw16.xor_u", i64_atomic_rmw16_xor_u, 0)
INSTRUCTION(0x40, "i64.atomic.rmw32.xor_u", i64_atomic_rmw32_xor_u, 0)

/* xchg */

INSTRUCTION(0x41, "i32.atomic.rmw.xchg", i32_atomic_rmw_xchg, 0)
INSTRUCTION(0x42, "i64.atomic.rmw.xchg", i64_atomic_rmw_xchg, 0)

INSTRUCTION(0x43, "i32.atomic.rmw8.xchg_u", i32_atomic_rmw8_xchg_u, 0)
INSTRUCTION(0x44, "i32.atomic.rmw16.xchg_u", i32_atomic_rmw16_xchg_u, 0)

INSTRUCTION(0x45, "i64.atomic.rmw8.xchg_u", i64_atomic_rmw8_xchg_u, 0)
INSTRUCTION(0x46, "i64.atomic.rmw16.xchg_u", i64_atomic_rmw16_xchg_u, 0)
INSTRUCTION(0x47, "i64.atomic.rmw32.xchg_u", i64_atomic_rmw32_xchg_u, 0)

/* cmpxchg */

INSTRUCTION(0x48, "i32.atomic.rmw.cmpxchg", i32_atomic_rmw_cmpxchg, 0)
INSTRUCTION(0x49, "i64.atomic.rmw.cmpxchg", i64_atomic_rmw_cmpxchg, 0)

INSTRUCTION(0x4a, "i32.atomic.rmw8.cmpxchg_u", i32_atomic_rmw8_cmpxchg_u, 0)
INSTRUCTION(0x4b, "i32.atomic.rmw16.cmpxchg_u", i32_atomic_rmw16_cmpxchg_u, 0)

INSTRUCTION(0x4c, "i64.atomic.rmw8.cmpxchg_u", i64_atomic_rmw8_cmpxchg_u, 0)
INSTRUCTION(0x4d, "i64.atomic.rmw16.cmpxchg_u", i64_atomic_rmw16_cmpxchg_u, 0)
INSTRUCTION(0x4e, "i64.atomic.rmw32.cmpxchg_u", i64_atomic_rmw32_cmpxchg_u, 0)

/* clang-format on */
