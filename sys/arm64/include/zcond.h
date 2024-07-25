#ifdef _KERNEL
#ifndef _MACHINE_ZCOND_H
#define _MACHINE_ZCOND_H

#include <sys/types.h>

static char nop_bytes[] = { 0x1f, 0x20, 0x03, 0xd5 };

#define ZCOND_NOP_ASM    \
	".byte 0x1f\n\t" \
	".byte 0x20\n\t" \
	".byte 0x03\n\t" \
	".byte 0xd5\n\t"
#define ZCOND_JMP_ASM	    "b"

#define ZCOND_MAX_INSN_SIZE 4

#endif
#endif
