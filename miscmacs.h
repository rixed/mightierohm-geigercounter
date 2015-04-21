// -*- c-basic-offset: 4; c-backslash-column: 79; indent-tabs-mode: nil -*-
// vim:sw=4 ts=4 sts=4 expandtab
#ifndef MISC_H_100406
#define MISC_H_100406

#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof(*(x)))
/// Compile time assertion
#define ASSERT_COMPILE(x) do { switch (0) { case 0: case (x):; } } while (0)

#define STRUCT_ALIGNMENT (sizeof(void *))
#define PAD_SIZE(x) (((x + (STRUCT_ALIGNMENT-1))/STRUCT_ALIGNMENT)*STRUCT_ALIGNMENT)

#define CHECK_LAST_FIELD(container, field_name, content) do { \
    ASSERT_COMPILE(sizeof(struct container) <= PAD_SIZE(offsetof(struct container, field_name) + sizeof (content))); \
} while (0)

/// Various utilities
#ifndef MAX
#   define MAX(a, b) (((a) >= (b) ? (a) : (b)))
#   define MIN(a, b) (((a) < (b) ? (a) : (b)))
#endif
#define NB_ELEMS(array) (sizeof array / sizeof array[0])
#define _STRIZE(arg) #arg
#define STRIZE(x)  _STRIZE(x)

/// Downcast from a subtype to a parent type (ie. from included struct to the struct that includes it)
#ifndef __NCC__ // for some reason ncc chocke on offsetof
#   include <stddef.h>
#   define DOWNCAST(val, member, subtype) ((struct subtype *)((char *)(val) - offsetof(struct subtype, member)))
#else
#   define DOWNCAST(val, member, subtype) ((struct subtype *)(val))
#endif

#define BIT(b) (1U << (b))
#define IS_BIT_SET(v, b) (!!((v) & BIT(b)))
#define BIT_SET(v, b) v |= BIT(b)
#define BIT_CLEAR(v, b) v &= ~BIT(b)
#define BIT_FLIP(v, b) v ^= BIT(b)

#define forever for (;;)
#define DIV_ROUND(a, b) (((a)+(a>>1))/b)

// Save us from linking in __divmodhi4 (assume __udivmodhi4 is already there)
div_t udiv(unsigned __num, unsigned __denom) __asm__("__udivmodhi4") __ATTR_CONST__;
ldiv_t uldiv(unsigned long __num, unsigned long __denom) __asm__("__udivmodsi4") __ATTR_CONST__;

#endif
