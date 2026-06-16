/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Marko Vlaić <mvlaic@freebsd.org>
 *
 * This code was developed as a Google Summer of Code 2024. project
 * under the guidance of Bojan Novković <bnovkov@freebsdorg>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef _KERNEL
#ifndef _SYS_ZCOND_H
#define _SYS_ZCOND_H

#ifdef ZCOND_PATCH

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>

#include <machine/zcond.h>

/*
 * The zcond interface provides a low-cost mechanism for conditional execution.
 * It is applicable in situations where branch selection is performed by
 * inspecting the state of a single boolean flag i.e blocks of the following
 * form: if(flag) {
 *      // do something
 *  }
 *
 * This kind of block is compiled into some sequence of load, test, jump
 * assembly instructions. The low cost provided by zcond is achieved by "baking
 * in" a single branch direction at compile time. This means outputting either
 * an unconditional jump or a nop, while the memory access is avoided.
 *
 * When the time comes to switch the branch direction, the current instruction
 * (jump or nop) is patched at runtime to a corresponding instruction (nop or
 * jump). Keep in mind that this is an expensive operation, since all cpus
 * except the one performing the patch need to be halted.
 *
 * Zconds expand boolean semantics with reference counting. A zcond is in a
 * false (disabled) state when its reference count is 0 and in a true (enabled)
 * state when the reference count is greater than 0.
 *
 * To use a zcond, first define it with: DEFINE_ZCOND_TRUE(name) or
 * DEFINE_ZCOND_FALSE(name) Alternatively, declare it with
 * DECLARE_ZCOND_TRUE(name) or DECLARE_ZCOND_FALSE(name). Then initialize it
 * with ZCOND_INIT(ZCOND_ENABLED) or ZCOND_INIT(ZCOND_DISABLED).
 *
 * Use zcond_branch_likely(cond) or zcond_branch_unlikely(cond) to perform branch selection
 * based on a zcond. Both functions execute a branch corresponding to the zcond state (true/false).
 * The likely/unlikely suffix is just a hint indicating which branch is expected to be
 * executed more frequently.
 *
 * To alter the state of a zcond, use zcond_enable(cond) and zcond_disable(cond).
 * These increase/decrease a zcond's reference count by 1 and perform the
 * instruction patch on the transition between false and true states.
 *
 * This header includes the interface intended to be used by consumers, as well
 * as some MI code. MD support can be found in sys/<arch>/include/zcond.h and
 * sys/<arch>/<arch>/zcond_machdep.c
 */

struct zcond {
	int refcnt;
	SLIST_HEAD(, patch_point) patch_points;
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

#define ZCOND_ELF_SECTION "set_zcond_patch_points_set"
#define ZCOND_LINKER_SET  zcond_patch_points_set


#ifdef __ILP32__
#define	_ZCOND_ASM_WORD			".long"
#else
#define	_ZCOND_ASM_WORD			".quad"
#endif

/*
 * __zcond_table is an ELF section which keeps
 * all the data related to the zcond mechanism.
 * A single entry describes a single patch_point.
 */
#define ZCOND_TABLE_ENTRY					\
	".pushsection " ZCOND_ELF_SECTION ", \"aw\" \n\t"	\
	_ZCOND_ASM_WORD " 1b \n\t"                              \
	_ZCOND_ASM_WORD " %l[l_true] \n\t"                      \
	_ZCOND_ASM_WORD " %c0 \n\t"                             \
	_ZCOND_ASM_WORD " 0 \n\t"                               \
	".popsection \n\t"

#define ZCOND_SET_START_STOP                                      \
	do {                                                      \
		__WEAK(__CONCAT(__start_set_, ZCOND_LINKER_SET)); \
		__WEAK(__CONCAT(__stop_set_, ZCOND_LINKER_SET));  \
	} while (0);

/*
 * Emits a __zcond_table entry, describing one patch_point.
 * Bakes in a nop instruction instruction, so the return value is initially
 * false.
 */
static __always_inline bool
zcond_nop(struct zcond *const zcond_p)
{
	ZCOND_SET_START_STOP
	asm goto("1: " ZCOND_NOP_ASM "\n\t" ZCOND_TABLE_ENTRY
		 :
		 : "i"(zcond_p)
		 :
		 : l_true);

	return (false);
l_true:
	return (true);
}

/*
 * Emits a __zcond_table entry, describing one patch_point.
 * Bakes in a jmp instruction instruction, so the return value is initially
 * true.
 */
static __always_inline bool
zcond_jmp(struct zcond *const zcond_p)
{
	ZCOND_SET_START_STOP
	asm goto("1:" ZCOND_JMP_ASM("%[l_true]") "\n\t" ZCOND_TABLE_ENTRY
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

#define ZCOND_INIT(enabled)						\
	{								\
		{                                                       \
			.patch_points = SLIST_HEAD_INITIALIZER(),       \
			.refcnt = (enabled)                             \
		}                                                       \
	}

#define ZCOND_ENABLED 1
#define ZCOND_DISABLED 0

#define DEFINE_ZCOND_TRUE(name)   struct zcond_true name = ZCOND_INIT(ZCOND_ENABLED)

#define DEFINE_ZCOND_FALSE(name)  struct zcond_false name = ZCOND_INIT(ZCOND_DISABLED)

#define DECLARE_ZCOND_TRUE(name)  struct zcond_true name;

#define DECLARE_ZCOND_FALSE(name) struct zcond_false name;

#define zcond_likely(x)   (__builtin_expect((x), 1))
#define zcond_unlikely(x) (__builtin_expect((x), 0))

/*
 * These macros inspect the state of a zcond (is it true or false)
 * thus instatiating a patch_point.
 */
#define zcond_branch_likely(cond_wrapped)					\
	({									\
		bool branch;                                                    \
		if (__builtin_types_compatible_p(typeof(cond_wrapped),          \
			struct zcond_true)) {                                   \
			branch = !zcond_nop(&(cond_wrapped.cond));              \
		} else if (__builtin_types_compatible_p(typeof(cond_wrapped),   \
			       struct zcond_false)) {                           \
			branch = !zcond_jmp(&(cond_wrapped.cond));              \
		}                                                               \
										\
		zcond_likely(branch);                                           \
	})

#define zcond_branch_unlikely(cond_wrapped)					\
	({									\
		bool branch;                                                    \
		if (__builtin_types_compatible_p(typeof(cond_wrapped),          \
			struct zcond_true)) {                                   \
			branch = zcond_jmp(&(cond_wrapped.cond));               \
		} else if (__builtin_types_compatible_p(typeof(cond_wrapped),   \
			       struct zcond_false)) {                           \
			branch = zcond_nop(&(cond_wrapped.cond));               \
		}                                                               \
										\
		zcond_unlikely(branch);                                         \
	})

/*
 * These macros change the state of a zcond.
 */
#define zcond_enable(cond_wrapped)  __zcond_toggle(&cond_wrapped.cond, true)
#define zcond_disable(cond_wrapped) __zcond_toggle(&cond_wrapped.cond, false)

/*
 * Change the state of a zcond by safely patching all of its
 * inspection points with appropriate instructions.
 */
void __zcond_toggle(struct zcond *cond, bool enable);

#else
// Fallback implementation when instruction patching is disabled

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>

struct zcond {
	int refcnt;
};

#define ZCOND_INIT(enabled)	\
{                               \
	.refcnt = enabled       \
}                               \

#define ZCOND_ENABLED 1
#define ZCOND_DISABLED 0

#define DEFINE_ZCOND_TRUE(name)   struct zcond name = ZCOND_INIT(ZCOND_ENABLED)

#define DEFINE_ZCOND_FALSE(name)  struct zcond name = ZCOND_INIT(ZCOND_DISABLED)

#define DECLARE_ZCOND_TRUE(name)  struct zcond name;

#define DECLARE_ZCOND_FALSE(name) struct zcond name;

#define zcond_likely(x)   (__builtin_expect((x), 1))
#define zcond_unlikely(x) (__builtin_expect((x), 0))

/*
 * These macros inspect the state of a zcond (is it true or false)
 * thus instatiating a patch_point.
 */
#define zcond_branch_likely(cond) (zcond_likely(cond.refcnt))
#define zcond_branch_unlikely(cond) (zcond_unlikely(cond.refcnt))

#define zcond_enable(cond)  __zcond_toggle(&cond, true)
#define zcond_disable(cond) __zcond_toggle(&cond, false)

void __zcond_toggle(struct zcond *cond, bool enable);

#endif // ZCOND_PATCH
#endif // _SYS_ZCOND_H
#endif // _KERNEL
