#ifndef _SYS_ZCOND_H
#define _SYS_ZCOND_H

#include <sys/types.h>

struct zcond {
    bool enabled; 
}; 

struct zcond_true {
    struct zcond cond;
};

struct zcond_false {
    struct zcond cond;
};

#define DEFINE_ZCOND_TRUE(name) \
    struct zcond_true name = {{ .enabled = true }}

#define DEFINE_ZCOND_FALSE(name) \
    struct zcond_false name = {{ .enabled = false }}

static __attribute__((always_inline)) zcond_nop() {
    asm goto(
            ".nops 8 \n\t"
            : : : : l_true );

    return false;
l_true: return true;
}

static __attribute__((always_inline)) zcond_jmp() {
    asm goto(
            "jmp %[l_true] \n\t"
            : : : : l_true );
    return false;
l_true: return true;
}

#define zcond_true(cond)                                                                \
    ({                                                                                  \
        bool branch;                                                                    \
        if (__builtin_types_compatible_p(typeof(cond), struct zcond_true)) {            \
            branch = zcond_jmp();                                                              \
        } else if (__builtin_types_compatible_p(typeof(cond), struct zcond_false)) {    \
            branch = zcond_nop();                                                             \
        }                                                                               \
                                                                                        \
        branch;                                                                         \
    })                        

#define zcond_false(cond)                                                               \
    ({                                                                                  \
        bool branch;                                                                    \
        if (__builtin_types_compatible_p(typeof(cond), struct zcond_true)) {            \
            branch = zcond_nop();                                                             \
        } else if (__builtin_types_compatible_p(typeof(cond), struct zcond_false)) {    \
            branch = zcond_jmp();                                                              \
        }                                                                               \
                                                                                        \
        branch;                                                                         \
    })                        

#endif
