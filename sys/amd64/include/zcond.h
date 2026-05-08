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
#ifndef _MACHINE_ZCOND_H
#define _MACHINE_ZCOND_H

#include <sys/types.h>

#define ZCOND_NOP_ASM     \
	".byte 0x0f \n\t" \
	".byte 0x1f \n\t" \
	".byte 0x44 \n\t" \
	".byte 0x00 \n\t" \
	".byte 0x00 \n\t"

#define ZCOND_JMP_ASM(target)	       "jmp " target

void
zcond_patchpoint_patch(uintptr_t patch_addr, uintptr_t lbl_true_addr);

#endif /*_MACHINE_ZCOND_H*/
#endif /* _KERNEL */
