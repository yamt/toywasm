/* clang-format off */

INSTRUCTION(0x00, "i32.trunc_sat_f32_s", i32_trunc_sat_f32_s, 0)
INSTRUCTION(0x01, "i32.trunc_sat_f32_u", i32_trunc_sat_f32_u, 0)
INSTRUCTION(0x02, "i32.trunc_sat_f64_s", i32_trunc_sat_f64_s, 0)
INSTRUCTION(0x03, "i32.trunc_sat_f64_u", i32_trunc_sat_f64_u, 0)
INSTRUCTION(0x04, "i64.trunc_sat_f32_s", i64_trunc_sat_f32_s, 0)
INSTRUCTION(0x05, "i64.trunc_sat_f32_u", i64_trunc_sat_f32_u, 0)
INSTRUCTION(0x06, "i64.trunc_sat_f64_s", i64_trunc_sat_f64_s, 0)
INSTRUCTION(0x07, "i64.trunc_sat_f64_u", i64_trunc_sat_f64_u, 0)

INSTRUCTION(0x08, "memory.init", memory_init, 0)
INSTRUCTION(0x09, "data.drop", data_drop, 0)
INSTRUCTION(0x0a, "memory.copy", memory_copy, 0)
INSTRUCTION(0x0b, "memory.fill", memory_fill, 0)

INSTRUCTION(0x0c, "table.init", table_init, 0)
INSTRUCTION(0x0d, "elem.drop", elem_drop, 0)
INSTRUCTION(0x0e, "table.copy", table_copy, 0)
INSTRUCTION(0x0f, "table.grew", table_grow, 0)
INSTRUCTION(0x10, "table.size", table_size, 0)
INSTRUCTION(0x11, "table.fill", table_fill, 0)

/* clang-format on */
