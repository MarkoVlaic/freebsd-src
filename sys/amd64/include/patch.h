#ifdef _KERNEL
#ifndef _MACHINE_PATCH_H
#define _MACHINE_PATCH_H

struct kpatch_md_ctxt {
	uint64_t cr3;
};

/*
 * Called before a single patch_point is patched.
 */
void kpatch_setup(vm_page_t, struct kpatch_md_ctxt *);

/*
 * Called after a single patch_point was patched.
 */
void kpatch_teardown(struct kpatch_md_ctxt *);

vm_offset_t kpatch_get_va(void);

bool kpatch_va_valid(vm_offset_t va);

#endif /* _MACHINE_PATCH_H */
#endif /* _KERNEL */
