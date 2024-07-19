#ifdef _KERNEL
#ifndef _MACHINE_ZCOND_H 
#define _MACHINE_ZCOND_H

#include <sys/types.h>

#define ZCOND_TABLE_ENTRY \
    ".pushsection __zcond_table, \"aw\" \n\t"  \
    ".quad 1b \n\t" \
    ".quad %l[l_true] \n\t" \
    ".quad %c0 \n\t" \
    ".quad 0 \n\t" \
    ".quad 0 \n\t" \
    ".popsection \n\t"

static char nop_bytes[] = { 0x1f, 0x20, 0x03, 0xd5 };
/*#define NOP_ASM \
    ".byte 0xd5\n\t" \
    ".byte 0x03\n\t" \
    ".byte 0x20\n\t" \
    ".byte 0x1f\n\t"
*/

#define NOP_ASM \
    ".byte 0x1f\n\t" \
    ".byte 0x20\n\t" \
    ".byte 0x03\n\t" \
    ".byte 0xd5\n\t"

#define MAX_INSN_SIZE 4

struct zcond;
struct ins_point;

static __attribute__((always_inline)) bool zcond_nop(struct zcond *const zcond_p) {
    asm goto(
            "1: " NOP_ASM
            ZCOND_TABLE_ENTRY
            : : "i" (zcond_p) : : l_true );

    return (false);
l_true: return (true);
}

static __attribute__((always_inline))  bool zcond_jmp(struct zcond *const zcond_p) {
    asm goto(
            "1: b %[l_true] \n\t"
            ZCOND_TABLE_ENTRY
            : : "i" (zcond_p) : : l_true );
    return (false);
l_true: return (true);
}


void zcond_before_patch(void);
void zcond_after_patch(void);

void zcond_get_patch_insn(struct ins_point *ins_p, unsigned char insn[], size_t *size);

#endif
#endif
