#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#if defined(USE_HOST_LOAD)
uintptr_t load(const uintptr_t **p)
        __attribute__((__import_module__("my-host-func")))
        __attribute__((__import_name__("load")));
#else
uintptr_t
load(const uintptr_t **p)
{
        return *(*p)++;
}
#endif

uintptr_t
load_add(const uintptr_t **p)
{
        uintptr_t a = load(p);
        uintptr_t b = load(p);
        return a + b;
}

typedef uintptr_t (*fn_t)(const uintptr_t **p);

uintptr_t
load_call(const uintptr_t **p)
{
        fn_t f = (void *)load(p);
        return f(p);
}

uintptr_t
load_call_add(const uintptr_t **p)
{
        uintptr_t a = load_call(p);
        uintptr_t b = load_call(p);
        return a + b;
}

// sum([1..16]) = 136
const uintptr_t table[] = {
        (uintptr_t)load_call_add,
        (uintptr_t)load_call_add,
        (uintptr_t)load_call_add,
        (uintptr_t)load_call_add,
        (uintptr_t)load_add,
        1,
        2,
        (uintptr_t)load_add,
        3,
        4,
        (uintptr_t)load_call_add,
        (uintptr_t)load_add,
        5,
        6,
        (uintptr_t)load_add,
        7,
        8,
        (uintptr_t)load_call,
        (uintptr_t)load_call_add,
        (uintptr_t)load_call_add,
        (uintptr_t)load_call,
        (uintptr_t)load_call,
        (uintptr_t)load_call,
        (uintptr_t)load_call,
        (uintptr_t)load_add,
        9,
        10,
        (uintptr_t)load_add,
        11,
        12,
        (uintptr_t)load_call_add,
        (uintptr_t)load,
        13,
        (uintptr_t)load_call,
        (uintptr_t)load_add,
        14,
        15,
        (uintptr_t)load_call,
        (uintptr_t)load_call,
        (uintptr_t)load_call,
        (uintptr_t)load,
        16,
};

int
main()
{
        const uintptr_t expected = 136;
        const uintptr_t *p = table;
        uintptr_t result = load_call(&p);
        assert(p - table == sizeof(table) / sizeof(table[0]));
        printf("result %ju (expected %ju)\n", (uintmax_t)result,
               (uintmax_t)expected);
        assert(result == expected);
}
