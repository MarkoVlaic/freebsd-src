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

#include <sys/types.h>
#include <sys/zcond.h>
#include <sys/pcpu.h>
#include <sys/kassert.h>

#include <machine/cpufunc.h>
#include <machine/zcond.h>
#include <machine/md_var.h>

static uint8_t insn[ZCOND_MAX_INSN_SIZE];

static uint8_t *
insn_nop(size_t size)
{
	if (size == ZCOND_INSN_SHORT_SIZE) {
		return &nop_short_bytes[0];
	}
	return &nop_long_bytes[0];
}

static uint8_t *
insn_jmp(size_t size, vm_offset_t patch_addr, vm_offset_t lbl_true_addr)
{
	int i;
	vm_offset_t offset;

	offset = lbl_true_addr - patch_addr - size;

	if (size == ZCOND_INSN_SHORT_SIZE) {
		insn[0] = ZCOND_JMP_SHORT_OPCODE;
		insn[1] = offset;
	} else {
		insn[0] = ZCOND_JMP_LONG_OPCODE;
		for (i = 0; i < ZCOND_INSN_LONG_SIZE - 1; i++) {
			insn[i + 1] = (offset >> (i * 8)) & 0xFF;
		}
	}

	return &insn[0];
}

void
zcond_before_patch(struct zcond_md_ctxt *ctxt)
{
	ctxt->wp = disable_wp();
}

void
zcond_after_patch(struct zcond_md_ctxt *ctxt)
{
	restore_wp(ctxt->wp);
}

uint8_t *
zcond_get_patch_insn(vm_offset_t patch_addr, vm_offset_t lbl_true_addr,
    size_t *size)
{
	uint8_t *pa;

	pa = (uint8_t *)patch_addr;
	if (*pa == nop_short_bytes[0]) {
		/* two byte nop */
		*size = ZCOND_INSN_SHORT_SIZE;
		return insn_jmp(*size, patch_addr, lbl_true_addr);
	} else if (*pa == nop_long_bytes[0]) {
		*size = ZCOND_INSN_LONG_SIZE;
		return insn_jmp(*size, patch_addr, lbl_true_addr);
	} else if (*pa == ZCOND_JMP_SHORT_OPCODE) {
		/* two byte jump */
		*size = ZCOND_INSN_SHORT_SIZE;
		return insn_nop(*size);
	} else if (*pa == ZCOND_JMP_LONG_OPCODE) {
		/* five byte jump */
		*size = ZCOND_INSN_LONG_SIZE;
		return insn_nop(*size);
	} else {
		panic("%s: unexpected opcode: %02hhx", __func__, *pa);
	}
}
