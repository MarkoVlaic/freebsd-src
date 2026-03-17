#ifdef _KERNEL
#ifndef ZCOND_H_
#define ZCOND_H_

#include <sys/types.h>

#define ZCOND_NOP_ASM  "nop"
#define ZCOND_JMP_ASM  "b"

void
zcond_patchpoint_patch(uintptr_t patch_addr, uintptr_t lbl_true_addr);

#endif /* ZCOND_H_ */
#endif /* _KERNEL  */
