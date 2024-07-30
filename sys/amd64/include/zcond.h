#ifdef _KERNEL
#ifndef _MACHINE_ZCOND_H
#define _MACHINE_ZCOND_H

// #include <sys/zcond.h>
#include <sys/types.h>

/* from Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 2B
 * 4-165 */
static char nop_short_bytes[] = { 0x66, 0x90 };
static char nop_long_bytes[] = { 0x0f, 0x1f, 0x44, 0x00, 0x00 };

#define ZCOND_NOP_ASM     \
	".byte 0x0f \n\t" \
	".byte 0x1f \n\t" \
	".byte 0x44 \n\t" \
	".byte 0x00 \n\t" \
	".byte 0x00 \n\t"

#define ZCOND_JMP_ASM	       "jmp"

#define ZCOND_JMP_SHORT_OPCODE 0xeb
#define ZCOND_JMP_LONG_OPCODE  0xe9

#define ZCOND_INSN_SHORT_SIZE  2
#define ZCOND_INSN_LONG_SIZE   5
#define ZCOND_MAX_INSN_SIZE    5

#endif /*_MACHINE_ZCOND_H*/
#endif /* _KERNEL */
