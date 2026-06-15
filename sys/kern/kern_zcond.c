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

#ifdef ZCOND_PATCH

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/linker_set.h>
#include <sys/refcount.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/zcond.h>

/*
 * Describes a single inspection of the zcond state (performed with an if
 * statement). Holds all the data neccessary to perform an instruction patch.
 */
struct patch_point {
	uintptr_t patch_addr;
	uintptr_t target;
	struct zcond *zcond;
	SLIST_ENTRY(patch_point) next;
} __attribute__((packed));

struct zcond_patch_arg {
	int patching_cpu;
	struct zcond *cond;
};

static void
zcond_load_patch_points(linker_file_t lf)
{
	struct patch_point *begin, *end;
	struct patch_point *pp;
	struct zcond *owning_zcond;

	if (linker_file_lookup_set(lf, __XSTRING(ZCOND_LINKER_SET), &begin,
		&end, NULL) == 0) {
		for (pp = begin; pp < end; pp++) {
			owning_zcond = pp->zcond;
			if (owning_zcond->patch_points.slh_first == NULL) {
				SLIST_INIT(&owning_zcond->patch_points);
			}

			SLIST_INSERT_HEAD(&owning_zcond->patch_points, pp,
			    next);
		}
	}
}

static void
zcond_unload_patch_points(linker_file_t lf)
{
	struct patch_point *begin, *end;
	struct patch_point *pp;

	if (linker_file_lookup_set(lf, __XSTRING(ZCOND_LINKER_SET), &begin,
	    &end, NULL) != 0) {
		return;
	}

	for(pp = begin; pp < end; pp++) {
		struct patch_point *pp2 = SLIST_FIRST(&pp->zcond->patch_points);
		if(pp2 == pp) {
			SLIST_REMOVE_HEAD(&pp->zcond->patch_points, next);
		} else if(pp2 != NULL) {
			struct patch_point *pp3;

			for(;;) {
				pp3 = SLIST_NEXT(pp2, next);
				if(pp3 == NULL) {
					break;
				}
				if(pp3 == pp) {
					SLIST_REMOVE_AFTER(pp2, next);
					break;
				}
				pp2 = pp3;
			}
		}
	}
}

static void
zcond_kld_load(void *arg __unused, struct linker_file *lf)
{
	zcond_load_patch_points(lf);
}

static void
zcond_kld_unload(void *arg __unused, struct linker_file *lf, int *error)
{
	if(*error != 0) {
		return;
	}

	zcond_unload_patch_points(lf);
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
	EVENTHANDLER_REGISTER(kld_unload_try, zcond_kld_unload, NULL, EVENTHANDLER_PRI_ANY);
	linker_file_foreach(zcond_load_patch_points_cb, NULL);
}

SYSINIT(zcond, SI_SUB_ZCOND, SI_ORDER_SECOND, zcond_init, NULL);


/*
 * Patch all patch_points belonging to cond.
 */
static void
zcond_patch(struct zcond *cond)
{
        struct patch_point *p;
	SLIST_FOREACH(p, &cond->patch_points, next) {
	  zcond_patchpoint_patch(p->patch_addr, p->target);
	}
}

static void
rendezvous_action(void *arg)
{
	struct zcond_patch_arg *data;

	data = (struct zcond_patch_arg *)arg;

	if (data->patching_cpu == curcpu) {
		zcond_patch(data->cond);
	}
}

void
__zcond_toggle(struct zcond *cond, bool enable)
{
	if(enable && refcount_acquire(&cond->refcnt) > 0) {
		// cond is already enabled
		return;
	} else if(!enable) {
		if(refcount_load(&cond->refcnt) == 0) {
			// cond is already disabled
			return;
		}

		if(refcount_release_if_not_last(&cond->refcnt)) {
			// cond stays disabled after release
			return;
		}
		
		// release the last reference to cond
		refcount_release(&cond->refcnt);
	}
	
	struct zcond_patch_arg arg = {
		.patching_cpu = curcpu,
		.cond = cond,
	};

	smp_rendezvous(NULL, rendezvous_action, NULL, &arg);
}

#else

#include <sys/zcond.h>
#include <sys/refcount.h>

void
__zcond_toggle(struct zcond *cond, bool enable)
{
	if(enable) {
		refcount_acquire(&cond->refcnt);
	} else {
		refcount_release(&cond->refcnt);
	}
}

#endif // ZCOND_PATCH
