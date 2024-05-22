#ifndef _SYS_ZCOND_H
#define _SYS_ZCOND_H

#include <sys/types.h>
#include <machine/zcond.h>

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



#define zcond_true(cond)                                                                \
    ({                                                                                  \
        bool branch;                                                                    \
        if (__builtin_types_compatible_p(typeof(cond), struct zcond_true)) {            \
            branch = arch_zcond_jmp();                                                              \
        } else if (__builtin_types_compatible_p(typeof(cond), struct zcond_false)) {    \
            branch = arch_zcond_nop();                                                             \
        }                                                                               \
                                                                                        \
        branch;                                                                         \
    })                        

#define zcond_false(cond)                                                               \
    ({                                                                                  \
        bool branch;                                                                    \
        if (__builtin_types_compatible_p(typeof(cond), struct zcond_true)) {            \
            branch = arch_zcond_nop();                                                             \
        } else if (__builtin_types_compatible_p(typeof(cond), struct zcond_false)) {    \
            branch = arch_zcond_jmp();                                                              \
        }                                                                               \
                                                                                        \
        branch;                                                                         \
    })                        

endif
