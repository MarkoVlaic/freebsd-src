#ifndef  ZCOND_MACHDEP_H
#define  ZCOND_MACHDEP_H

#include <sys/types.h>

#define ZCOND_NOP_ASM		".option push; .option norvc; nop; .option pop"
#define ZCOND_JMP_ASM(target)	".option push; .option norvc; j " target "; .option pop"

void
zcond_patchpoint_patch(uintptr_t patch_addr, uintptr_t lbl_true_addr);

#endif //  ZCOND_MACHDEP_H
