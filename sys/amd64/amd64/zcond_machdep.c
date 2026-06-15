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
#include <sys/pcpu.h>
#include <sys/kassert.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpufunc.h>
#include <machine/zcond.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>

#define INSN_SHORT_SIZE  2
#define INSN_LONG_SIZE   5
#define INSN_MAX_SIZE 5

#define JMP_SHORT_OPCODE 0xeb
#define JMP_LONG_OPCODE  0xe9

/* from Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 2B
 * 4-165 */
static const uint8_t nop_short_bytes[INSN_SHORT_SIZE] = { 0x66, 0x90 };
static const uint8_t nop_long_bytes[INSN_LONG_SIZE] = { 0x0f, 0x1f, 0x44, 0x00, 0x00 };

static uint8_t insn[INSN_MAX_SIZE];

static size_t
insn_size(uintptr_t addr)
{

	uint8_t *paddr;

	paddr = (uint8_t *)addr;
	if (*paddr == nop_short_bytes[0]) {
		/* two byte nop */
		return (INSN_SHORT_SIZE);
	} else if (*paddr == nop_long_bytes[0]) {
		return (INSN_LONG_SIZE);
	} else if (*paddr == JMP_SHORT_OPCODE) {
		/* two byte jump */
		return (INSN_SHORT_SIZE);
	} else if (*paddr == JMP_LONG_OPCODE) {
		/* five byte jump */
		return (INSN_LONG_SIZE);
	}

	panic("%s: unexpected opcode: %02hhx", __func__, *paddr);
}

static const uint8_t *
insn_nop(size_t size)
{
	if (size == INSN_SHORT_SIZE) {
		return &nop_short_bytes[0];
	}
	return &nop_long_bytes[0];
}

static const uint8_t *
insn_jmp(size_t size, uintptr_t patch_addr, uintptr_t target)
{
	int i;
	uintptr_t offset;

	offset = target - patch_addr - size;

	if (size == INSN_SHORT_SIZE) {
		insn[0] = JMP_SHORT_OPCODE;
		insn[1] = offset;
	} else {
		insn[0] = JMP_LONG_OPCODE;
		for (i = 0; i < INSN_LONG_SIZE - 1; i++) {
			insn[i + 1] = (offset >> (i * 8)) & 0xFF;
		}
	}

	return &insn[0];
}

static const uint8_t *
get_patch_insn(uintptr_t patch_addr, uintptr_t target,
    size_t *size)
{
	const uint8_t *pa;

	*size = insn_size(patch_addr);
	pa = (uint8_t *)patch_addr;
	if (*pa == nop_short_bytes[0]) {
		/* two byte nop */
		return insn_jmp(*size, patch_addr, target);
	} else if (*pa == nop_long_bytes[0]) {
		return insn_jmp(*size, patch_addr, target);
	} else if (*pa == JMP_SHORT_OPCODE) {
		/* two byte jump */
		return insn_nop(*size);
	} else if (*pa == JMP_LONG_OPCODE) {
		/* five byte jump */
		return insn_nop(*size);
	} else {
		panic("%s: unexpected opcode: %02hhx", __func__, *pa);
	}
}


static bool
patchpoint_valid(uintptr_t patch_addr, uintptr_t target)
{
	if(patch_addr < KERNSTART || target < KERNSTART)
		return (false);

	size_t size = insn_size(patch_addr);
	if(patch_addr == target || patch_addr + size < patch_addr)
		return (false);

	intptr_t offset = target - patch_addr - size;
	if(offset < -(1l << 31) || offset > (1l << 31))
		return (false);

	return (true);
}

void
zcond_patchpoint_patch(uintptr_t patch_addr, uintptr_t target)
{

	KASSERT(patchpoint_valid(patch_addr, target),
	    ("%s: invalid zcond patchpoint %#jx -> %#jx",
	    __func__, (uintmax_t)patch_addr, target));

	size_t size;
	const uint8_t *insn;
	bool wp;

	insn = get_patch_insn(patch_addr, target, &size);
	wp = disable_wp();
	memcpy((void *)patch_addr, insn, size);
	restore_wp(wp);
}
