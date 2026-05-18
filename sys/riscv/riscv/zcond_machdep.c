/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Marko Vlaić <mvlaic@freebsd.org>
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

#include <sys/systm.h>
#include <sys/sdt.h>

#include <machine/encoding.h>
#include <machine/zcond.h>

#define INSN_SIZE 4

static const uint32_t nop_insn = 0x13;

static bool
patch_addr_valid(uintptr_t patch_addr, uintptr_t lbl_true_addr)
{
	int64_t offset;

	if (patch_addr == lbl_true_addr ||
	    (patch_addr & (INSN_C_SIZE - 1)) != 0 ||
	    (lbl_true_addr & (INSN_C_SIZE - 1)) != 0)
		return (false);
	offset = lbl_true_addr - patch_addr;
	if (offset < -(1 << 19) || offset > (1 << 19))
		return (false);
	return (true);
}

void
zcond_patchpoint_patch(uintptr_t patch_addr, uintptr_t lbl_true_addr)
{
	uint32_t instr;
	int32_t imm;

	KASSERT(patch_addr_valid(patch_addr, lbl_true_addr),
	    ("%s: invalid tracepoint %#lx -> %#lx",
	    __func__, patch_addr, lbl_true_addr));

	if(*((uint32_t*)patch_addr) == nop_insn) {
		/* Replace nop with jump */
		imm = lbl_true_addr - patch_addr;
		imm = (imm & 0x100000) |
		      ((imm & 0x7fe) << 8) |
		      ((imm & 0x800) >> 2) |
		      ((imm & 0xff000) >> 12);
		instr = (imm << 12) | MATCH_JAL;
	} else {
		/* Replace jump with nop */
		instr = nop_insn;
	}

	memcpy((void *)patch_addr, &instr, sizeof(instr));
	fence_i();
}
