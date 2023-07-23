#if !defined(_VALTYPE_H)
#define _VALTYPE_H

enum valtype {
        /* numtype */
        TYPE_i32 = 0x7f,
        TYPE_i64 = 0x7e,
        TYPE_f32 = 0x7d,
        TYPE_f64 = 0x7c,

        /* vectype */
        TYPE_v128 = 0x7b,

        /* reftype */
        TYPE_FUNCREF = 0x70,
        TYPE_EXTERNREF = 0x6f,

        /* pseudo types for validation logic */
        TYPE_ANYREF = 0xfe, /* any reftype */
        TYPE_UNKNOWN = 0xff,
};

#endif /* !defined(_VALTYPE_H) */
