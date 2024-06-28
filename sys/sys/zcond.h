#ifndef _SYS_ZCOND_H
#define _SYS_ZCOND_H

#include <sys/types.h>
#include <machine/zcond.h>
#include <sys/param.h>
#include <vm/vm.h>
#include <vm/vm_page.h>

struct ins_point {
    vm_offset_t patch_addr; /* address of the nop or jmp instruction to be patched */
    vm_offset_t lbl_true_addr; /* address of the label to jump to when the condition is true */
    struct zcond* zcond;
    SLIST_ENTRY(ins_point) next;
    vm_offset_t swap_page;
};

struct zcond {
    bool enabled; 
    SLIST_HEAD(, ins_point) ins_points;
    vm_offset_t swap_page_vaddr;
}; 

struct zcond_true {
    struct zcond cond;
};

struct zcond_false {
    struct zcond cond;
};

#define DEFINE_ZCOND_TRUE(name) \
    struct zcond_true name = {{ .enabled = true, .ins_points = SLIST_HEAD_INITIALIZER(), .swap_page_addr = NULL }}

#define DEFINE_ZCOND_FALSE(name) \
    struct zcond_false name = {{ .enabled = false, .ins_points = SLIST_HEAD_INITIALIZER(), .swap_page_addr = NULL }}

#define ZCOND_INIT(zcond_wrapped) \
    SLIST_INIT(&zcond_wrapped->cond->ins_points)


#define zcond_true(cond_wrapped) \
    ({                                                                                  \
        bool branch;                                                                    \
        if (__builtin_types_compatible_p(typeof(cond_wrapped), struct zcond_true)) {            \
            branch = arch_zcond_jmp(&(cond_wrapped.cond));                                                              \
        } else if (__builtin_types_compatible_p(typeof(cond_wrapped), struct zcond_false)) {    \
            branch = arch_zcond_nop(&(cond_wrapped.cond));                                                             \
        }                                                                               \
                                                                                        \
        branch;                                                                         \
    })                        

#define zcond_false(cond_wrapped)                                                               \
    ({                                                                                  \
        bool branch;                                                                    \
        if (__builtin_types_compatible_p(typeof(cond_wrapped), struct zcond_true)) {            \
            branch = arch_zcond_nop(&(cond_wrapped.cond));                                                             \
        } else if (__builtin_types_compatible_p(typeof(cond_wrapped), struct zcond_false)) {    \
            branch = arch_zcond_jmp(&(cond_wrapped.cond));                                                              \
        }                                                                               \
                                                                                        \
        branch;                                                                         \
    })                        

//void zcond_enable(struct zcond *cond);
#define zcond_disable(cond_wrapped) __zcond_disable(&cond_wrapped.cond)
void __zcond_disable(struct zcond *cond);

#endif
