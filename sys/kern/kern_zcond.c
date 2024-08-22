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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpuset.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/refcount.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/zcond.h>
#include <sys/patch.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <machine/atomic.h>
#include <machine/cpufunc.h>

struct patch_point {
	vm_offset_t patch_addr;
	vm_offset_t lbl_true_addr;
	struct zcond *zcond;
	SLIST_ENTRY(patch_point) next;
} __attribute__((packed));


MALLOC_DECLARE(M_ZCOND);
MALLOC_DEFINE(M_ZCOND, "zcond", "malloc for the zcond subsystem");

static void
zcond_load_patch_points(linker_file_t lf)
{
	struct patch_point *begin, *end;
	struct patch_point *ins_p;
	struct zcond *owning_zcond;

	if (linker_file_lookup_set(lf, __XSTRING(ZCOND_LINKER_SET), &begin,
		&end, NULL) == 0) {
		for (ins_p = begin; ins_p < end; ins_p++) {
			owning_zcond = ins_p->zcond;

			if (owning_zcond->patch_points.slh_first == NULL) {
				SLIST_INIT(&owning_zcond->patch_points);
				owning_zcond->refcnt = 1;
				owning_zcond->num_patch_points = 0;
			}

			SLIST_INSERT_HEAD(&owning_zcond->patch_points, ins_p,
			    next);
			owning_zcond->num_patch_points++;
		}
	}
}

static void
zcond_kld_load(void *arg __unused, struct linker_file *lf)
{
	zcond_load_patch_points(lf);
}

static int
zcond_load_patch_points_cb(linker_file_t lf, void *arg __unused)
{
	zcond_load_patch_points(lf);
	return (0);
}

/*
 * Collect patch_points from the __zcond_table ELF section into a list.
 * Prepare a CPU local copy of the kernel_pmap, used to safely patch
 * an instruction.
 */
static void
zcond_init(const void *unused)
{
	EVENTHANDLER_REGISTER(kld_load, zcond_kld_load, NULL,
	    EVENTHANDLER_PRI_ANY);
	linker_file_foreach(zcond_load_patch_points_cb, NULL);
}
SYSINIT(zcond, SI_SUB_PATCH, SI_ORDER_THIRD, zcond_init, NULL);


void
__zcond_toggle(struct zcond *cond, bool enable)
{
	vm_offset_t *vas;
	uint8_t **insns;
	uint8_t *insn;
	size_t *sizes;
	size_t insn_size;
	size_t cnt;
	struct patch_point *p;
	int i;

	if (enable && refcount_acquire(&cond->refcnt) > 1) {
		return;
	} else if (!enable) {
		if (!refcount_release_if_not_last(&cond->refcnt) ||
		    refcount_load(&cond->refcnt) != 1) {
			return;
		}
	}
	
	cnt = cond->num_patch_points;
	vas = malloc(cnt * sizeof(vm_offset_t), M_ZCOND, M_WAITOK);
	insns = malloc(cnt * sizeof(uint8_t *), M_ZCOND, M_WAITOK);	
	sizes = malloc(cnt * sizeof(size_t), M_ZCOND, M_WAITOK);

	i = 0;
	SLIST_FOREACH(p, &cond->patch_points, next) {
		vas[i] = p->patch_addr;
		insn = zcond_get_patch_insn(p->patch_addr, p->lbl_true_addr, &insn_size);
		insns[i] = malloc(sizeof(uint8_t) * insn_size, M_ZCOND, M_WAITOK);
		memcpy(insns[i], insn, insn_size);
		sizes[i] = insn_size;
		i++;
	}

	kpatch_text_batch(vas, insns, sizes, cnt);

	i = 0;
	SLIST_FOREACH(p, &cond->patch_points, next) {
		free(insns[i], M_ZCOND);
		i++;	
	}	

	free(vas, M_ZCOND);
	free(insns, M_ZCOND);
	free(sizes, M_ZCOND);
}

/*
 * Testing code.
 */
DEFINE_ZCOND_TRUE(cond1);
DEFINE_ZCOND_FALSE(cond2);

static int
trigger_zcond_test(SYSCTL_HANDLER_ARGS)
{
	struct sbuf buf;
	sbuf_new_for_sysctl(&buf, NULL, 256, req);

	sbuf_printf(&buf, "zcond test start\n");
	if (zcond_true(cond1)) {
		sbuf_printf(&buf, "cond 1 true\n");
	} else {
		sbuf_printf(&buf, "cond 1 false\n");
	}

	if (zcond_true(cond2)) {
		sbuf_printf(&buf, "cond2 true\n");
	} else {
		sbuf_printf(&buf, "cond2 false\n");
	}

	sbuf_finish(&buf);
	sbuf_delete(&buf);

	return (0);
}

static int
trigger_zcond_test2(SYSCTL_HANDLER_ARGS)
{
	struct sbuf buf;
	sbuf_new_for_sysctl(&buf, NULL, 256, req);

	sbuf_printf(&buf, "zcond test 2 start\n");
	if (zcond_true(cond1)) {
		sbuf_printf(&buf, "cond 1 true %s\n", __func__);
	}

	// simulate long jump with nops
	/*asm (
	    ".nops 512\n\t":::
	);*/

	sbuf_finish(&buf);
	sbuf_delete(&buf);
	return (0);
}

static int
trigger_zcond_test3(SYSCTL_HANDLER_ARGS)
{
	struct sbuf buf;
	sbuf_new_for_sysctl(&buf, NULL, 256, req);

	sbuf_printf(&buf, "zcond test 3 start\n");
	if (zcond_false(cond1)) {
		sbuf_printf(&buf, "cond1 false\n");
	} else {
		sbuf_printf(&buf, "cond1 true\n");
	}

	if (zcond_false(cond2)) {
		sbuf_printf(&buf, "cond2 false\n");
	} else {
		sbuf_printf(&buf, "cond2 true\n");
	}

	sbuf_finish(&buf);
	sbuf_delete(&buf);

	return (0);
}

static int
zcond_list_inspection_points(SYSCTL_HANDLER_ARGS)
{
	struct sbuf buf;
	sbuf_new_for_sysctl(&buf, NULL, 1024, req);

	printf("&cond1 = %#08lx | &cond2 = %#08lx\n", (unsigned long)&cond1,
	    (unsigned long)&cond2);

	sbuf_printf(&buf, "inspection points for cond1:\n");
	struct patch_point *p;
	SLIST_FOREACH(p, &cond1.cond.patch_points, next) {
		sbuf_printf(&buf,
		    "patch_addr = %#08lx | jump_addr = %#08lx | zcond_ptr = %p\n",
		    p->patch_addr, p->lbl_true_addr, p->zcond);
	}

	sbuf_printf(&buf, "inspection points for cond2:\n");
	SLIST_FOREACH(p, &cond2.cond.patch_points, next) {
		sbuf_printf(&buf,
		    "patch_addr = %#08lx | jump_addr = %#08lx | zcond_ptr = %p\n",
		    p->patch_addr, p->lbl_true_addr, p->zcond);
	}

	sbuf_finish(&buf);
	sbuf_delete(&buf);

	return (0);
}

static int
zcond1_disable(SYSCTL_HANDLER_ARGS)
{
	struct sbuf buf;
	sbuf_new_for_sysctl(&buf, NULL, 256, req);

	zcond_disable(cond1);
	sbuf_printf(&buf, "cond1 disabled\n");

	return (0);
}

static int
zcond1_enable(SYSCTL_HANDLER_ARGS)
{
	struct sbuf buf;
	sbuf_new_for_sysctl(&buf, NULL, 256, req);

	zcond_enable(cond1);
	sbuf_printf(&buf, "cond1 enabled\n");

	sbuf_finish(&buf);
	sbuf_delete(&buf);
	return (0);
}

static int
zcond2_disable(SYSCTL_HANDLER_ARGS)
{
	struct sbuf buf;
	sbuf_new_for_sysctl(&buf, NULL, 256, req);

	zcond_disable(cond2);
	sbuf_printf(&buf, "cond2 disabled\n");

	return (0);
}

static int
zcond2_enable(SYSCTL_HANDLER_ARGS)
{
	struct sbuf buf;
	sbuf_new_for_sysctl(&buf, NULL, 256, req);

	zcond_enable(cond2);
	sbuf_printf(&buf, "cond2 enabled\n");

	sbuf_finish(&buf);
	sbuf_delete(&buf);

	return (0);
}
SYSCTL_PROC(_kern, OID_AUTO, zcond, CTLFLAG_RD | CTLTYPE_STRING, NULL, 0,
    trigger_zcond_test, "I", "trigger zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond2, CTLFLAG_RD | CTLTYPE_STRING,
    SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test2, "I",
    "trigger second zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond3, CTLFLAG_RD | CTLTYPE_STRING,
    SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test3, "I",
    "trigger third zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond_ins_p, CTLFLAG_RD | CTLTYPE_STRING,
    SYSCTL_NULL_INT_PTR, 0, zcond_list_inspection_points, "I",
    "list cond1 inspection points");
SYSCTL_PROC(_kern, OID_AUTO, zcond1_enable, CTLFLAG_RD | CTLTYPE_STRING,
    SYSCTL_NULL_INT_PTR, 0, zcond1_enable, "I", "enable zcond1");
SYSCTL_PROC(_kern, OID_AUTO, zcond1_disable, CTLFLAG_RD | CTLTYPE_STRING,
    SYSCTL_NULL_INT_PTR, 0, zcond1_disable, "I", "disable zcond1");
SYSCTL_PROC(_kern, OID_AUTO, zcond2_enable, CTLFLAG_RD | CTLTYPE_STRING,
    SYSCTL_NULL_INT_PTR, 0, zcond2_enable, "I", "enable zcond2");
SYSCTL_PROC(_kern, OID_AUTO, zcond2_disable, CTLFLAG_RD | CTLTYPE_STRING,
    SYSCTL_NULL_INT_PTR, 0, zcond2_disable, "I", "disable zcond2");
