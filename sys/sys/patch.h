#ifdef _KERNEL
#ifndef SYS_PATCH_H
#define SYS_PATCH_H

/*
 * Forward declaration of a struct, defined separately for each architecture in
 * <machine/patch.h>
 */

struct patch_md_ctxt;

void kpatch_text_single(vm_offset_t va, uint8_t *insn, size_t size);
void kpatch_text_batch(vm_offset_t *vas, uint8_t **insns, size_t *sizes, size_t cnt);

#endif
#endif
