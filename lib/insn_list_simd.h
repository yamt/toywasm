/* clang-format off */

/* prefix: 0xfd */

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#load */
INSTRUCTION(0x00, "v128.load", v128_load, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#load-and-extend */
/* https://webassembly.github.io/spec/core/exec/instructions.html#exec-load-extend */
INSTRUCTION(0x01, "v128.load8x8_s", v128_load8x8_s, 0)
INSTRUCTION(0x02, "v128.load8x8_u", v128_load8x8_u, 0)
INSTRUCTION(0x03, "v128.load16x4_s", v128_load16x4_s, 0)
INSTRUCTION(0x04, "v128.load16x4_u", v128_load16x4_u, 0)
INSTRUCTION(0x05, "v128.load32x2_s", v128_load32x2_s, 0)
INSTRUCTION(0x06, "v128.load32x2_u", v128_load32x2_u, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#load-and-splat */
INSTRUCTION(0x07, "v128.load8_splat", v128_load8_splat, 0)
INSTRUCTION(0x08, "v128.load16_splat", v128_load16_splat, 0)
INSTRUCTION(0x09, "v128.load32_splat", v128_load32_splat, 0)
INSTRUCTION(0x0a, "v128.load64_splat", v128_load64_splat, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#store */
INSTRUCTION(0x0b, "v128.store", v128_store, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#constant */
INSTRUCTION(0x0c, "v128.const", v128_const, INSN_FLAG_CONST)

#if 0
/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#shuffling-using-immediate-indices */
INSTRUCTION(0x0d, "i8x16.shuffle", i8x16_shuffle, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#swizzling-using-variable-indices */
INSTRUCTION(0x0e, "i8x16.swizzle", i8x16_swizzle, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#create-vector-with-identical-lanes */
INSTRUCTION(0x0f, "i8x16.splat", i8x16_splat, 0)
INSTRUCTION(0x10, "i16x8.splat", i16x8_splat, 0)
INSTRUCTION(0x11, "i32x4.splat", i32x4_splat, 0)
INSTRUCTION(0x12, "i64x2.splat", i64x2_splat, 0)
INSTRUCTION(0x13, "f32x4.splat", f32x4_splat, 0)
INSTRUCTION(0x14, "f64x2.splat", f64x2_splat, 0)
#endif

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#extract-lane-as-a-scalar */
INSTRUCTION(0x15, "i8x16.extract_lane_s", i8x16_extract_lane_s, 0)
INSTRUCTION(0x16, "i8x16.extract_lane_u", i8x16_extract_lane_u, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#replace-lane-value */
INSTRUCTION(0x17, "i8x16.replace_lane", i8x16_replace_lane, 0)

INSTRUCTION(0x18, "i16x8.extract_lane_s", i16x8_extract_lane_s, 0)
INSTRUCTION(0x19, "i16x8.extract_lane_u", i16x8_extract_lane_u, 0)
INSTRUCTION(0x1a, "i16x8.replace_lane", i16x8_replace_lane, 0)

INSTRUCTION(0x1b, "i32x4.extract_lane", i32x4_extract_lane, 0)
INSTRUCTION(0x1c, "i32x4.replace_lane", i32x4_replace_lane, 0)

INSTRUCTION(0x1d, "i64x2.extract_lane", i64x2_extract_lane, 0)
INSTRUCTION(0x1e, "i64x2.replace_lane", i64x2_replace_lane, 0)

INSTRUCTION(0x1f, "f32x4.extract_lane", f32x4_extract_lane, 0)
INSTRUCTION(0x20, "f32x4.replace_lane", f32x4_replace_lane, 0)

INSTRUCTION(0x21, "f64x2.extract_lane", f64x2_extract_lane, 0)
INSTRUCTION(0x22, "f64x2.replace_lane", f64x2_replace_lane, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#comparisons */
INSTRUCTION(0x23, "i8x16.eq", i8x16_eq, 0)
INSTRUCTION(0x24, "i8x16.ne", i8x16_ne, 0)
INSTRUCTION(0x25, "i8x16.lt_s", i8x16_lt_s, 0)
INSTRUCTION(0x26, "i8x16.lt_u", i8x16_lt_u, 0)
INSTRUCTION(0x27, "i8x16.gt_s", i8x16_gt_s, 0)
INSTRUCTION(0x28, "i8x16.gt_u", i8x16_gt_u, 0)
INSTRUCTION(0x29, "i8x16.le_s", i8x16_le_s, 0)
INSTRUCTION(0x2a, "i8x16.le_u", i8x16_le_u, 0)
INSTRUCTION(0x2b, "i8x16.ge_s", i8x16_ge_s, 0)
INSTRUCTION(0x2c, "i8x16.ge_u", i8x16_ge_u, 0)

INSTRUCTION(0x2d, "i16x8.eq", i16x8_eq, 0)
INSTRUCTION(0x2e, "i16x8.ne", i16x8_ne, 0)
INSTRUCTION(0x2f, "i16x8.lt_s", i16x8_lt_s, 0)
INSTRUCTION(0x30, "i16x8.lt_u", i16x8_lt_u, 0)
INSTRUCTION(0x31, "i16x8.gt_s", i16x8_gt_s, 0)
INSTRUCTION(0x32, "i16x8.gt_u", i16x8_gt_u, 0)
INSTRUCTION(0x33, "i16x8.le_s", i16x8_le_s, 0)
INSTRUCTION(0x34, "i16x8.le_u", i16x8_le_u, 0)
INSTRUCTION(0x35, "i16x8.ge_s", i16x8_ge_s, 0)
INSTRUCTION(0x36, "i16x8.ge_u", i16x8_ge_u, 0)

INSTRUCTION(0x37, "i32x4.eq", i32x4_eq, 0)
INSTRUCTION(0x38, "i32x4.ne", i32x4_ne, 0)
INSTRUCTION(0x39, "i32x4.lt_s", i32x4_lt_s, 0)
INSTRUCTION(0x3a, "i32x4.lt_u", i32x4_lt_u, 0)
INSTRUCTION(0x3b, "i32x4.gt_s", i32x4_gt_s, 0)
INSTRUCTION(0x3c, "i32x4.gt_u", i32x4_gt_u, 0)
INSTRUCTION(0x3d, "i32x4.le_s", i32x4_le_s, 0)
INSTRUCTION(0x3e, "i32x4.le_u", i32x4_le_u, 0)
INSTRUCTION(0x3f, "i32x4.ge_s", i32x4_ge_s, 0)
INSTRUCTION(0x40, "i32x4.ge_u", i32x4_ge_u, 0)

INSTRUCTION(0x41, "f32x4.eq", f32x4_eq, 0)
INSTRUCTION(0x42, "f32x4.ne", f32x4_ne, 0)
INSTRUCTION(0x43, "f32x4.lt", f32x4_lt, 0)
INSTRUCTION(0x44, "f32x4.gt", f32x4_gt, 0)
INSTRUCTION(0x45, "f32x4.le", f32x4_le, 0)
INSTRUCTION(0x46, "f32x4.ge", f32x4_ge, 0)

INSTRUCTION(0x47, "f64x2.eq", f64x2_eq, 0)
INSTRUCTION(0x48, "f64x2.ne", f64x2_ne, 0)
INSTRUCTION(0x49, "f64x2.lt", f64x2_lt, 0)
INSTRUCTION(0x4a, "f64x2.gt", f64x2_gt, 0)
INSTRUCTION(0x4b, "f64x2.le", f64x2_le, 0)
INSTRUCTION(0x4c, "f64x2.ge", f64x2_ge, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#bitwise-operations */
INSTRUCTION(0x4d, "v128.not", v128_not, 0)
INSTRUCTION(0x4e, "v128.and", v128_and, 0)
INSTRUCTION(0x4f, "v128.andnot", v128_andnot, 0)
INSTRUCTION(0x50, "v128.or", v128_or, 0)
INSTRUCTION(0x51, "v128.xor", v128_xor, 0)
INSTRUCTION(0x52, "v128.bitselect", v128_bitselect, 0)

INSTRUCTION(0x60, "i8x16.abs", i8x16_abs, 0)
INSTRUCTION(0x61, "i8x16.neg", i8x16_neg, 0)

INSTRUCTION(0x63, "i8x16.all_true", i8x16_all_true, 0)
INSTRUCTION(0x64, "i8x16.bitmask", i8x16_bitmask, 0)
/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#integer-to-integer-narrowing */
INSTRUCTION(0x65, "i8x16.narrow_i16x8_s", i8x16_narrow_i16x8_s, 0)
INSTRUCTION(0x66, "i8x16.narrow_i16x8_u", i8x16_narrow_i16x8_u, 0)

INSTRUCTION(0x6b, "i8x16.shl", i8x16_shl, 0)
INSTRUCTION(0x6c, "i8x16.shr_s", i8x16_shr_s, 0)
INSTRUCTION(0x6d, "i8x16.shr_u", i8x16_shr_u, 0)
INSTRUCTION(0x6e, "i8x16.add", i8x16_add, 0)
#if 0
INSTRUCTION(0x6f, "i8x16.add_sat_s", i8x16_add_sat_s, 0)
INSTRUCTION(0x70, "i8x16.add_sat_u", i8x16_add_sat_u, 0)
#endif
INSTRUCTION(0x71, "i8x16.sub", i8x16_sub, 0)
#if 0
INSTRUCTION(0x72, "i8x16.sub_sat_s", i8x16_sub_sat_s, 0)
INSTRUCTION(0x73, "i8x16.sub_sat_u", i8x16_sub_sat_u, 0)
#endif

INSTRUCTION(0x76, "i8x16.min_s", i8x16_min_s, 0)
INSTRUCTION(0x77, "i8x16.min_u", i8x16_min_u, 0)
INSTRUCTION(0x78, "i8x16.max_s", i8x16_max_s, 0)
INSTRUCTION(0x79, "i8x16.max_u", i8x16_max_u, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#lane-wise-integer-rounding-average */
INSTRUCTION(0x7b, "i8x16.avgr_u", i8x16_avgr_u, 0)

INSTRUCTION(0x80, "i16x8.abs", i16x8_abs, 0)
INSTRUCTION(0x81, "i16x8.neg", i16x8_neg, 0)

INSTRUCTION(0x83, "i16x8.all_true", i16x8_all_true, 0)
INSTRUCTION(0x84, "i16x8.bitmask", i16x8_bitmask, 0)
INSTRUCTION(0x85, "i16x8.narrow_i32x4_s", i16x8_narrow_i32x4_s, 0)
INSTRUCTION(0x86, "i16x8.narrow_i32x4_u", i16x8_narrow_i32x4_u, 0)

INSTRUCTION(0x87, "i16x8.extend_low_i8x16_s", i16x8_extend_low_i8x16_s, 0)
INSTRUCTION(0x88, "i16x8.extend_high_i8x16_s", i16x8_extend_high_i8x16_s, 0)
INSTRUCTION(0x89, "i16x8.extend_low_i8x16_u", i16x8_extend_low_i8x16_u, 0)
INSTRUCTION(0x8a, "i16x8.extend_high_i8x16_u", i16x8_extend_high_i8x16_u, 0)

INSTRUCTION(0x8b, "i16x8.shl", i16x8_shl, 0)
INSTRUCTION(0x8c, "i16x8.shr_s", i16x8_shr_s, 0)
INSTRUCTION(0x8d, "i16x8.shr_u", i16x8_shr_u, 0)
INSTRUCTION(0x8e, "i16x8.add", i16x8_add, 0)
#if 0
INSTRUCTION(0x8f, "i16x8.add_sat_s", i16x8_add_sat_s, 0)
INSTRUCTION(0x90, "i16x8.add_sat_u", i16x8_add_sat_u, 0)
#endif
INSTRUCTION(0x91, "i16x8.sub", i16x8_sub, 0)
#if 0
INSTRUCTION(0x92, "i16x8.sub_sat_s", i16x8_sub_sat_s, 0)
INSTRUCTION(0x93, "i16x8.sub_sat_u", i16x8_sub_sat_u, 0)
#endif

INSTRUCTION(0x95, "i16x8.mul", i16x8_mul, 0)
INSTRUCTION(0x96, "i16x8.min_s", i16x8_min_s, 0)
INSTRUCTION(0x97, "i16x8.min_u", i16x8_min_u, 0)
INSTRUCTION(0x98, "i16x8.max_s", i16x8_max_s, 0)
INSTRUCTION(0x99, "i16x8.max_u", i16x8_max_u, 0)

INSTRUCTION(0x9b, "i16x8.avgr_u", i16x8_avgr_u, 0)

INSTRUCTION(0xa0, "i32x4.abs", i32x4_abs, 0)
INSTRUCTION(0xa1, "i32x4.neg", i32x4_neg, 0)

INSTRUCTION(0xa3, "i32x4.all_true", i32x4_all_true, 0)
INSTRUCTION(0xa4, "i32x4.bitmask", i32x4_bitmask, 0)

INSTRUCTION(0xa7, "i32x4.extend_low_i16x8_s", i32x4_extend_low_i16x8_s, 0)
INSTRUCTION(0xa8, "i32x4.extend_high_i16x8_s", i32x4_extend_high_i16x8_s, 0)
INSTRUCTION(0xa9, "i32x4.extend_low_i16x8_u", i32x4_extend_low_i16x8_u, 0)
INSTRUCTION(0xaa, "i32x4.extend_high_i16x8_u", i32x4_extend_high_i16x8_u, 0)

INSTRUCTION(0xab, "i32x4.shl", i32x4_shl, 0)
INSTRUCTION(0xac, "i32x4.shr_s", i32x4_shr_s, 0)
INSTRUCTION(0xad, "i32x4.shr_u", i32x4_shr_u, 0)
INSTRUCTION(0xae, "i32x4.add", i32x4_add, 0)

INSTRUCTION(0xb1, "i32x4.sub", i32x4_sub, 0)

INSTRUCTION(0xb5, "i32x4.mul", i32x4_mul, 0)

INSTRUCTION(0xb6, "i32x4.min_s", i32x4_min_s, 0)
INSTRUCTION(0xb7, "i32x4.min_u", i32x4_min_u, 0)
INSTRUCTION(0xb8, "i32x4.max_s", i32x4_max_s, 0)
INSTRUCTION(0xb9, "i32x4.max_u", i32x4_max_u, 0)

#if 0
/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#integer-dot-product */
INSTRUCTION(0xba, "i32x4.dot_i16x8_s", i32x4_dot_i16x8_s, 0)
#endif

INSTRUCTION(0xc0, "i64x2.abs", i64x2_abs, 0)
INSTRUCTION(0xc1, "i64x2.neg", i64x2_neg, 0)

INSTRUCTION(0xc4, "i64x2.bitmask", i64x2_bitmask, 0)

INSTRUCTION(0xc7, "i64x2.extend_low_i32x4_s", i64x2_extend_low_i32x4_s, 0)
INSTRUCTION(0xc8, "i64x2.extend_high_i32x4_s", i64x2_extend_high_i32x4_s, 0)
INSTRUCTION(0xc9, "i64x2.extend_low_i32x4_u", i64x2_extend_low_i32x4_u, 0)
INSTRUCTION(0xca, "i64x2.extend_high_i32x4_u", i64x2_extend_high_i32x4_u, 0)

INSTRUCTION(0xcb, "i64x2.shl", i64x2_shl, 0)
INSTRUCTION(0xcc, "i64x2.shr_s", i64x2_shr_s, 0)
INSTRUCTION(0xcd, "i64x2.shr_u", i64x2_shr_u, 0)
INSTRUCTION(0xce, "i64x2.add", i64x2_add, 0)

INSTRUCTION(0xd1, "i64x2.sub", i64x2_sub, 0)

INSTRUCTION(0xd5, "i64x2.mul", i64x2_mul, 0)

#if 0
/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#integer-dot-product */
INSTRUCTION(0xba, "i64x2.dot_i16x8_s", i64x2_dot_i16x8_s, 0)
#endif

INSTRUCTION(0x67, "f32x4.ceil", f32x4_ceil, 0)
INSTRUCTION(0x68, "f32x4.floor", f32x4_floor, 0)
INSTRUCTION(0x69, "f32x4.trunc", f32x4_trunc, 0)
INSTRUCTION(0x6a, "f32x4.nearest", f32x4_nearest, 0)

INSTRUCTION(0x74, "f64x2.ceil", f64x2_ceil, 0)
INSTRUCTION(0x75, "f64x2.floor", f64x2_floor, 0)
INSTRUCTION(0x7a, "f64x2.trunc", f64x2_trunc, 0)
INSTRUCTION(0x94, "f64x2.nearest", f64x2_nearest, 0)

INSTRUCTION(0xe0, "f32x4.abs", f32x4_abs, 0)
INSTRUCTION(0xe1, "f32x4.neg", f32x4_neg, 0)
INSTRUCTION(0xe3, "f32x4.sqrt", f32x4_sqrt, 0)
INSTRUCTION(0xe4, "f32x4.add", f32x4_add, 0)
INSTRUCTION(0xe5, "f32x4.sub", f32x4_sub, 0)
INSTRUCTION(0xe6, "f32x4.mul", f32x4_mul, 0)
INSTRUCTION(0xe7, "f32x4.div", f32x4_div, 0)
INSTRUCTION(0xe8, "f32x4.min", f32x4_min, 0)
INSTRUCTION(0xe9, "f32x4.max", f32x4_max, 0)
INSTRUCTION(0xea, "f32x4.pmin", f32x4_pmin, 0)
INSTRUCTION(0xeb, "f32x4.pmax", f32x4_pmax, 0)

INSTRUCTION(0xec, "f64x2.abs", f64x2_abs, 0)
INSTRUCTION(0xed, "f64x2.neg", f64x2_neg, 0)
INSTRUCTION(0xef, "f64x2.sqrt", f64x2_sqrt, 0)
INSTRUCTION(0xf0, "f64x2.add", f64x2_add, 0)
INSTRUCTION(0xf1, "f64x2.sub", f64x2_sub, 0)
INSTRUCTION(0xf2, "f64x2.mul", f64x2_mul, 0)
INSTRUCTION(0xf3, "f64x2.div", f64x2_div, 0)
INSTRUCTION(0xf4, "f64x2.min", f64x2_min, 0)
INSTRUCTION(0xf5, "f64x2.max", f64x2_max, 0)
INSTRUCTION(0xf6, "f64x2.pmin", f64x2_pmin, 0)
INSTRUCTION(0xf7, "f64x2.pmax", f64x2_pmax, 0)

#if 0
INSTRUCTION(0xf8, "i32x4.trunc_sat_f32x4_s", i32x4_trunc_sat_f32x4_s, 0)
INSTRUCTION(0xf9, "i32x4.trunc_sat_f32x4_u", i32x4_trunc_sat_f32x4_u, 0)
#endif

INSTRUCTION(0xfa, "f32x4.convert_i32x4_s", f32x4_convert_i32x4_s, 0)
INSTRUCTION(0xfb, "f32x4.convert_i32x4_u", f32x4_convert_i32x4_u, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#load-and-zero-pad */
INSTRUCTION(0x5c, "v128.load32_zero", v128_load32_zero, 0)
INSTRUCTION(0x5d, "v128.load64_zero", v128_load64_zero, 0)

#if 0
/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#extended-integer-multiplication */
INSTRUCTION(0x9c, "i16x8.extmul_low_i8x16_s", i16x8_extmul_low_i8x16_s, 0)
INSTRUCTION(0x9d, "i16x8.extmul_high_i8x16_s", i16x8_extmul_high_i8x16_s, 0)
INSTRUCTION(0x9e, "i16x8.extmul_low_i8x16_u", i16x8_extmul_low_i8x16_u, 0)
INSTRUCTION(0x9f, "i16x8.extmul_high_i8x16_u", i16x8_extmul_high_i8x16_u, 0)

INSTRUCTION(0xbc, "i32x4.extmul_low_i16x8_s", i32x4_extmul_low_i16x8_s, 0)
INSTRUCTION(0xbd, "i32x4.extmul_high_i16x8_s", i32x4_extmul_high_i16x8_s, 0)
INSTRUCTION(0xbe, "i32x4.extmul_low_i16x8_u", i32x4_extmul_low_i16x8_u, 0)
INSTRUCTION(0xbf, "i32x4.extmul_high_i16x8_u", i32x4_extmul_high_i16x8_u, 0)

INSTRUCTION(0xdc, "i64x2.extmul_low_i32x4_s", i64x2_extmul_low_i32x4_s, 0)
INSTRUCTION(0xdd, "i64x2.extmul_high_i32x4_s", i64x2_extmul_high_i32x4_s, 0)
INSTRUCTION(0xde, "i64x2.extmul_low_i32x4_u", i64x2_extmul_low_i32x4_u, 0)
INSTRUCTION(0xdf, "i64x2.extmul_high_i32x4_u", i64x2_extmul_high_i32x4_u, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#saturating-integer-q-format-rounding-multiplication */
INSTRUCTION(0x82, "i16x8.q15mulr_sat_s", i16x8_q15mulr_sat_s, 0)
#endif

INSTRUCTION(0x53, "v128.any_true", v128_any_true, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#load-lane */
INSTRUCTION(0x54, "v128.load8_lane", v128_load8_lane, 0)
INSTRUCTION(0x55, "v128.load16_lane", v128_load16_lane, 0)
INSTRUCTION(0x56, "v128.load32_lane", v128_load32_lane, 0)
INSTRUCTION(0x57, "v128.load64_lane", v128_load64_lane, 0)

INSTRUCTION(0x58, "v128.store8_lane", v128_store8_lane, 0)
INSTRUCTION(0x59, "v128.store16_lane", v128_store16_lane, 0)
INSTRUCTION(0x5a, "v128.store32_lane", v128_store32_lane, 0)
INSTRUCTION(0x5b, "v128.store64_lane", v128_store64_lane, 0)

INSTRUCTION(0xd6, "i64x2.eq", i64x2_eq, 0)
INSTRUCTION(0xd7, "i64x2.ne", i64x2_ne, 0)
INSTRUCTION(0xd8, "i64x2.lt_s", i64x2_lt_s, 0)
INSTRUCTION(0xd9, "i64x2.gt_s", i64x2_gt_s, 0)
INSTRUCTION(0xda, "i64x2.le_s", i64x2_le_s, 0)
INSTRUCTION(0xdb, "i64x2.ge_s", i64x2_ge_s, 0)

INSTRUCTION(0xc3, "i64x2.all_true", i64x2_all_true, 0)

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#integer-to-double-precision-floating-point */
INSTRUCTION(0xfe, "f64x2.convert_low_i32x4_s", f64x2_convert_low_i32x4_s, 0)
INSTRUCTION(0xff, "f64x2.convert_low_i32x4_u", f64x2_convert_low_i32x4_u, 0)

#if 0
/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#double-precision-floating-point-to-integer-with-saturation */
INSTRUCTION(0xfc, "i32x4.trunc_sat_f64x2_s_zero", i32x4_trunc_sat_f64x2_s_zero, 0)
INSTRUCTION(0xfd, "i32x4.trunc_sat_f64x2_u_zero", i32x4_trunc_sat_f64x2_u_zero, 0)
#endif

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#double-precision-floating-point-to-single-precision */
INSTRUCTION(0x5e, "f32x4.demote_f64x2_zero", f32x4_demote_f64x2_zero, 0)
/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#single-precision-floating-point-to-double-precision */
INSTRUCTION(0x5f, "f64x2.promote_low_f32x4", f64x2_promote_low_f32x4, 0)

#if 0
/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#lane-wise-population-count */
INSTRUCTION(0x62, "i8x16.popcnt", i8x16_popcnt, 0)
#endif

/* https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#extended-pairwise-integer-addition */
INSTRUCTION(0x7c, "i16x8.extadd_pairwise_i8x16_s", i16x8_extadd_pairwise_i8x16_s, 0)
INSTRUCTION(0x7d, "i16x8.extadd_pairwise_i8x16_u", i16x8_extadd_pairwise_i8x16_u, 0)

INSTRUCTION(0x7e, "i32x4.extadd_pairwise_i16x8_s", i32x4_extadd_pairwise_i16x8_s, 0)
INSTRUCTION(0x7f, "i32x4.extadd_pairwise_i16x8_u", i32x4_extadd_pairwise_i16x8_u, 0)

/* clang-format on */
