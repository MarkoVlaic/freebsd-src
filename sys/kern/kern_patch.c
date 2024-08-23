#include <sys/types.h>
#include <sys/cpuset.h>
#include <sys/patch.h>
#include <sys/types.h>
#include <sys/smp.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <machine/patch.h>

struct patch_arg {
	int patching_cpu;
	vm_offset_t *vas;
	uint8_t **insns;
	size_t *sizes;
	size_t cnt;
	struct kpatch_md_ctxt md_ctxt;
};

static vm_offset_t patch_addr;

static void
patch_init(void *arg)
{
	patch_addr = kpatch_get_va();
}

SYSINIT(patch, SI_SUB_PATCH, SI_ORDER_SECOND, patch_init, NULL);

static void
__patch(void *arg) {
	struct patch_arg *data;
	vm_offset_t va;
	vm_page_t patch_page;
	uint8_t *insn;
	size_t size;
	size_t page_overflow;
	int i;

	data = (struct patch_arg *)arg;
	for(i = 0; i < data->cnt; i++) {
		va = data->vas[i];
		insn = data->insns[i];
		size = data->sizes[i];
		
		if(!kpatch_va_valid(va)) {
			panic("%s: va %lx not inside .text section", __func__, va);
		}

		if((va & (~PAGE_MASK)) != ((va + size) & (~PAGE_MASK))) {
			page_overflow = (va + size) & PAGE_MASK;
		} else {
			page_overflow = 0;
		}

		KASSERT((page_overflow < PAGE_SIZE && page_overflow > 0), ("%s: patching instruction over more than 2 pages", __func__));
		
		patch_page = PHYS_TO_VM_PAGE(vtophys(va));
		kpatch_setup(patch_page, &data->md_ctxt);
		memcpy((void *)(patch_addr + (va & PAGE_MASK)), insn,
		    size - page_overflow);
		kpatch_teardown(&data->md_ctxt);

		if(page_overflow != 0) {
			va += size - page_overflow;
			insn += size - page_overflow;
			patch_page = PHYS_TO_VM_PAGE(vtophys(va));

			kpatch_setup(patch_page, &data->md_ctxt);
			memcpy((void *)(patch_addr + (va & PAGE_MASK)), insn, page_overflow);
			kpatch_teardown(&data->md_ctxt);
		}

	}
}

static void
rendezvous_action(void *arg)
{
	struct patch_arg *data;

	data = (struct patch_arg *)arg;

	if (data->patching_cpu == curcpu) {
		__patch(data);
	}
}

void
kpatch_text_single(vm_offset_t va, uint8_t *insn, size_t size)
{
	kpatch_text_batch(&va, &insn, &size, 1);
}

void
kpatch_text_batch(vm_offset_t *vas, uint8_t **insns, size_t *sizes, size_t cnt)
{
	struct patch_arg arg = {
		.patching_cpu = curcpu,
		.vas = vas,
		.insns = insns,
		.sizes = sizes,
		.cnt = cnt
	};

	smp_rendezvous(NULL, rendezvous_action, NULL, &arg);
}
