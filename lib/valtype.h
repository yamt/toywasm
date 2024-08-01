#if !defined(_TOYWASM_VALTYPE_H)
#define _TOYWASM_VALTYPE_H

/*
 * Note: these values are sN-encoded negative values so that
 * they can be distinguished from typeidx in eg. block types.
 */

#define _S7(x) (0x7f & (uint8_t)(x))

enum valtype {
        /* numtype */
        TYPE_i32 = _S7(-0x01),
        TYPE_i64 = _S7(-0x02),
        TYPE_f32 = _S7(-0x03),
        TYPE_f64 = _S7(-0x04),

        /* vectype */
        TYPE_v128 = _S7(-0x05),

        /* reftype */
        TYPE_EXNREF = _S7(-0x17),
        TYPE_FUNCREF = _S7(-0x10),
        TYPE_EXTERNREF = _S7(-0x11),

        /* pseudo types for validation logic */
        TYPE_ANYREF = 0xfe, /* any reftype */
        TYPE_UNKNOWN = 0xff,
};

#endif /* !defined(_TOYWASM_VALTYPE_H) */
