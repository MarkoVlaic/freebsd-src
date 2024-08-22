#ifdef _KERNEL
#ifndef _MACHINE_PATCH_H
#define _MACHINE_PATCH_H

struct patch_md_ctxt {
	uint64_t cr3;
};

/*
 * Called before a single patch_point is patched.
 */
void before_patch(vm_page_t, struct patch_md_ctxt *);

/*
 * Called after a single patch_point was patched.
 */
void after_patch(struct patch_md_ctxt *);

vm_offset_t patch_get_va(void);

bool kpatch_va_in_text(vm_offset_t va);

#endif /* _MACHINE_ZCOND_H */
#endif /* _KERNEL */
