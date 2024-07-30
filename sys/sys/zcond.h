#ifdef _KERNEL
#ifndef _SYS_ZCOND_H
#define _SYS_ZCOND_H

#include <sys/types.h>
#include <sys/param.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <machine/zcond.h>

/*
 * Describes a single inspection of the zcond state (performed with an if
 * statement). Holds all the data neccessary to perform a safe instruction
 * patch.
 */
struct ins_point {
	vm_offset_t patch_addr; /* address of the nop or jmp instruction to be
				   patched */
	vm_offset_t lbl_true_addr; /* address of the label to jump to when the
				      condition is true */
	struct zcond *
	    zcond; /* pointer to the zcond inspected by this inspection point */
	SLIST_ENTRY(ins_point) next;
	vm_offset_t
	    mirror_address; /* virtual address used to perform a safe patch */
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
 * __zcond_table is an ELF section which keeps
 * all the data related to the zcond mechanism.
 * A single entry describes a single ins_point.
 */
#define ZCOND_TABLE_ENTRY                         \
	".pushsection __zcond_table, \"aw\" \n\t" \
	".quad 1b \n\t"                           \
	".quad %l[l_true] \n\t"                   \
	".quad %c0 \n\t"                          \
	".quad 0 \n\t"                            \
	".quad 0 \n\t"                            \
	".popsection \n\t"

/*
 * Emits a __zcond_table entry, describing one ins_point.
 * Bakes in a nop instruction instruction, so the return value is initially
 * false.
 */
static __attribute__((always_inline)) bool
zcond_nop(struct zcond *const zcond_p)
{
	asm goto("1: " ZCOND_NOP_ASM ZCOND_TABLE_ENTRY
		 :
		 : "i"(zcond_p)
		 :
		 : l_true);

	return (false);
l_true:
	return (true);
}

/*
 * Emits a __zcond_table entry, describing one ins_point.
 * Bakes in a jmp instruction instruction, so the return value is initially
 * true.
 */
static __attribute__((always_inline)) bool
zcond_jmp(struct zcond *const zcond_p)
{
	asm goto("1:" ZCOND_JMP_ASM " %[l_true] \n\t" ZCOND_TABLE_ENTRY
		 :
		 : "i"(zcond_p)
		 :
		 : l_true);
	return (false);
l_true:
	return (true);
}

/*
 * These macros declare and initialize a new zcond.
 */

#define ZCOND_INIT(state)  { { .enabled = (state), \
	    .ins_points = SLIST_HEAD_INITIALIZER() } }

#define DEFINE_ZCOND_TRUE(name)                       \
	struct zcond_true name = ZCOND_INIT(true)

#define DEFINE_ZCOND_FALSE(name)                        \
	struct zcond_false name = ZCOND_INIT(false)

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

/*
 * Called before a single ins_point is patched.
 */
void zcond_before_patch(void);

/*
 * Called after a single ins_point was patched.
 */
void zcond_after_patch(void);

/*
 * Called before CPUs are parked. Use this hook to perform MD pmap loading
 * and other MD setup.
 */
void zcond_before_rendezvous(void);

/*
 * Called after the whole zcond is patched and CPUs are resumed.
 *  Use this hook to perform MD pmap cleanup.
 */
void zcond_after_rendezvous(void);

/*
 * Calculates the bytes of instruction with which the ins_p inspection point is
 * to be patched with. insn[] is populated with the instruction bytes and size
 * is set to the number of instruction bytes.
 */
void zcond_get_patch_insn(struct ins_point *ins_p, unsigned char insn[],
    size_t *size);
#endif
#endif
