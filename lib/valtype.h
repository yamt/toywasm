#if !defined(_TOYWASM_VALTYPE_H)
#define _TOYWASM_VALTYPE_H

/*
 * Note: these values are sN-encoded negative values so that
 * they can be distinguished from typeidx in eg. block types.
 */

enum valtype {
        /* numtype */
        TYPE_i32 = 0x7f, /* -0x01 */
        TYPE_i64 = 0x7e, /* -0x02 */
        TYPE_f32 = 0x7d, /* -0x03 */
        TYPE_f64 = 0x7c, /* -0x04 */

        /* vectype */
        TYPE_v128 = 0x7b, /* -0x05 */

        /* reftype */
        TYPE_EXNREF = 0x69,    /* -0x17 */
        TYPE_FUNCREF = 0x70,   /* -0x10 */
        TYPE_EXTERNREF = 0x6f, /* -0x11 */

        /* pseudo types for validation logic */
        TYPE_ANYREF = 0xfe, /* any reftype */
        TYPE_UNKNOWN = 0xff,
};

#endif /* !defined(_TOYWASM_VALTYPE_H) */
