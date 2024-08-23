#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/domainset.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/vmem.h>
#include <sys/zcond.h>
#include <sys/linker.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/uma.h>
#include <vm/vm_domainset.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_pagequeue.h>

#include <machine/cpufunc.h>
#include <machine/pmap.h>
#include <machine/patch.h>

static struct pmap patch_pmap;
static vm_offset_t patch_va;
static pt_entry_t *patch_pte;

static pt_entry_t *
patch_init_pte(void)
{
	vm_page_t dummy_page;
	int domain;
	pml5_entry_t *pml5e;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_pindex_t pml5_idx, pml4_idx, pdp_idx, pd_idx;
	vm_paddr_t mphys;
	extern int la57;
	bool is_la57;

	domain = PCPU_GET(domain);
	vmem_alloc(VM_DOMAIN(domain)->vmd_kernel_nofree_arena, PAGE_SIZE,
	    M_BESTFIT | M_WAITOK, &patch_va);
	dummy_page = vm_page_alloc_noobj_domain(domain,
	    VM_ALLOC_WIRED | VM_ALLOC_NOFREE);
	pmap_enter(&patch_pmap, patch_va, dummy_page, VM_PROT_WRITE,
	    PMAP_ENTER_WIRED, 0);

	is_la57 = false;
	if (patch_pmap.pm_type == PT_X86) {
		is_la57 = la57;
	}

	pml4_idx = pmap_pml4e_index(patch_va);
	if (is_la57) {
		pml5_idx = pmap_pml5e_index(patch_va);
		pml5e = &patch_pmap.pm_pmltopu[pml5_idx];
		KASSERT(*pml5e != 0,
		    ("%s: va %#jx pml5e == 0", __func__, patch_va));
		mphys = *pml5e & PG_FRAME;

		pml4e = (pml4_entry_t *)PHYS_TO_DMAP(mphys);
		pml4e = &pml4e[pml4_idx];
	} else {
		pml4e = &patch_pmap.pm_pmltop[pml4_idx];
	}

	KASSERT(*pml4e != 0,
	    ("%s: va %#jx pml4e == 0", __func__, patch_va));
	mphys = *pml4e & PG_FRAME;

	pdpe = (pdp_entry_t *)PHYS_TO_DMAP(mphys);
	pdp_idx = pmap_pdpe_index(patch_va);
	pdpe += pdp_idx;
	KASSERT(*pdpe != 0,
	    ("%s: va %#jx pdpe == 0", __func__, patch_va));
	mphys = *pdpe & PG_FRAME;

	pde = (pd_entry_t *)PHYS_TO_DMAP(mphys);
	pd_idx = pmap_pde_index(patch_va);
	pde += pd_idx;
	KASSERT(*pde != 0, ("%s: va %#jx pde == 0", __func__, patch_va));
	mphys = *pde & PG_FRAME;

	pte = (pt_entry_t *)PHYS_TO_DMAP(mphys);
	pte += pmap_pte_index(patch_va);

	return (pte);
}

static void
patch_pmap_init(const void *unused)
{
	extern char stext;
	vm_offset_t text_start;
	size_t copy_size;

	text_start = (vm_offset_t) &stext;
	copy_size = linker_kernel_file->size - (text_start - (vm_offset_t) linker_kernel_file->address);

	printf("kernel addr offset = %lx | text start = %lx | kernel size = %zu | copy size = %zu\n", (vm_offset_t) linker_kernel_file->address, text_start, linker_kernel_file->size, copy_size);
	memset(&patch_pmap, 0, sizeof(patch_pmap));
	PMAP_LOCK_INIT(&patch_pmap);
	pmap_pinit(&patch_pmap);
	pmap_copy(&patch_pmap, kernel_pmap, text_start, PAGE_SIZE,
	    text_start);

	patch_pte = patch_init_pte();
}
SYSINIT(patch_pmap, SI_SUB_PATCH, SI_ORDER_FIRST, patch_pmap_init, NULL);

static void
patch_qenter(vm_page_t m)
{
	pt_entry_t pa;
	int cache_bits;

	cache_bits = pmap_cache_bits(&patch_pmap, m->md.pat_mode, false);
	pa = VM_PAGE_TO_PHYS(m) | cache_bits;
	if ((*patch_pte & (PG_FRAME | X86_PG_PTE_CACHE)) != pa) {
		pte_store(patch_pte,
		    pa | pg_nx | X86_PG_A | X86_PG_M | X86_PG_RW | X86_PG_V);
	}

	invlpg(patch_va);
}

vm_offset_t
kpatch_get_va(void)
{
	return (patch_va);
}

void
kpatch_setup(vm_page_t patch_page, struct kpatch_md_ctxt *ctxt)
{
	patch_qenter(patch_page);
	ctxt->cr3 = rcr3();
	load_cr3(patch_pmap.pm_cr3);
}

void
kpatch_teardown(struct kpatch_md_ctxt *ctxt)
{
	mfence();
	load_cr3(ctxt->cr3);
	pte_clear(patch_pte);
	invltlb();
}

bool
kpatch_va_valid(vm_offset_t va)
{
	extern char stext, etext;
	vm_offset_t start, end;

	start = (vm_offset_t) &stext;
	end = (vm_offset_t) &etext;

	return (va >= start && va <= end);
}
