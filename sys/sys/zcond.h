#ifndef _SYS_ZCOND_H
#define _SYS_ZCOND_H

#include <sys/types.h>
#include <machine/zcond.h>

struct ins_point {
    vm_offset_t patch_addr; /* address of the nop or jmp instruction to be patched */
    vm_offset_t lbl_true_addr; /* address of the label to jump to when the condition is true */
    struct zcond* zcond;
    SLIST_ENTRY(ins_point) next;
};

struct zcond {
    bool enabled; 
    SLIST_HEAD(, ins_point) ins_points;
}; 

struct zcond_true {
    struct zcond cond;
};

struct zcond_false {
    struct zcond cond;
};

#define DEFINE_ZCOND_TRUE(name) \
    struct zcond_true name = {{ .enabled = true, ins_points = SLIST_HEAD_INITIALIZER() }}

#define DEFINE_ZCOND_FALSE(name) \
    struct zcond_false name = {{ .enabled = false, ins_points = SLIST_HEAD_INITIALIZER() }}

#define ZCOND_INIT(zcond_wrapped) \
    SLIST_INIT(&zcond_wrapped->cond->ins_points)

#define zcond_true(cond) \
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

#endif
