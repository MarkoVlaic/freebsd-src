#ifdef _KERNEL
#ifndef _MACHINE_ZCOND_H
#define _MACHINE_ZCOND_H

#include <sys/types.h>

#define ZCOND_NOP_ASM  "nop"
#define ZCOND_JMP_ASM  "b"

void
zcond_patchpoint_patch(uintptr_t patch_addr, uintptr_t lbl_true_addr);

#endif /* _MACHINE_ZCOND_H */
#endif /* _KERNEL  */
