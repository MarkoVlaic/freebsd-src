#ifdef _KERNEL
#ifndef _SYS_ZCOND_H
#define _SYS_ZCOND_H

#include <sys/types.h>
#include <sys/param.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <machine/zcond.h>

/*
 * Describes a single inspection of the zcond state (performed with an if statement).
 * Holds all the data neccessary to perform a safe instruction patch.
*/
struct ins_point {
	vm_offset_t patch_addr; /* address of the nop or jmp instruction to be
				   patched */
	vm_offset_t lbl_true_addr; /* address of the label to jump to when the
				      condition is true */
	struct zcond *zcond; /* pointer to the zcond inspected by this inspection point */
	SLIST_ENTRY(ins_point) next; 
	vm_offset_t mirror_address; /* virtual address used to perform a safe patch */
} __attribute__((packed));

/*
 * A single optimized boolean.
*/
struct zcond {
	bool enabled;
	SLIST_HEAD(, ins_point) ins_points;
};

/*
 * Wrapper types are needed for compile time decision making.
*/
struct zcond_true {
	struct zcond cond;
};

struct zcond_false {
	struct zcond cond;
};

/*
 * These macros declare and initialize a new zcond.
*/
#define DEFINE_ZCOND_TRUE(name)                       \
	struct zcond_true name = { { .enabled = true, \
	    .ins_points = SLIST_HEAD_INITIALIZER() } }

#define DEFINE_ZCOND_FALSE(name)                        \
	struct zcond_false name = { { .enabled = false, \
	    .ins_points = SLIST_HEAD_INITIALIZER() } }

/*
 * These macros inspect the state of a zcond (is it true or false)
 * thus instatiating an ins_point.
*/
#define zcond_true(cond_wrapped)                                              \
	({                                                                    \
		bool branch;                                                  \
		if (__builtin_types_compatible_p(typeof(cond_wrapped),        \
			struct zcond_true)) {                                 \
			branch = zcond_jmp(&(cond_wrapped.cond));             \
		} else if (__builtin_types_compatible_p(typeof(cond_wrapped), \
			       struct zcond_false)) {                         \
			branch = zcond_nop(&(cond_wrapped.cond));             \
		}                                                             \
                                                                              \
		branch;                                                       \
	})

#define zcond_false(cond_wrapped)                                             \
	({                                                                    \
		bool branch;                                                  \
		if (__builtin_types_compatible_p(typeof(cond_wrapped),        \
			struct zcond_true)) {                                 \
			branch = zcond_nop(&(cond_wrapped.cond));             \
		} else if (__builtin_types_compatible_p(typeof(cond_wrapped), \
			       struct zcond_false)) {                         \
			branch = zcond_jmp(&(cond_wrapped.cond));             \
		}                                                             \
                                                                              \
		branch;                                                       \
	})

/*
 * These macros change the state of a zcond.
*/
#define zcond_enable(cond_wrapped) __zcond_set_enabled(&cond_wrapped.cond, true)
#define zcond_disable(cond_wrapped) \
	__zcond_set_enabled(&cond_wrapped.cond, false)

/*
 * Change the state of a zcond by safely patching all of its
 * inspection points with appropriate instructions.
*/
void __zcond_set_enabled(struct zcond *cond, bool new_state);

#endif
#endif
