#ifndef _MACHINE_ZCOND_H 
#define _MACHINE_ZCOND_H

//#include <sys/zcond.h>
#include <sys/types.h>

#define ZCOND_TABLE_ENTRY \
    ".pushsection __zcond_table, \"aw\" \n\t"  \
    ".quad 1b \n\t" \
    ".quad %l[l_true] \n\t" \
    ".quad %c0 \n\t" \
    ".quad 0 \n\t" \
    ".quad 0 \n\t" \
    ".popsection \n\t"

// from Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 2B 4-165 
static char nop2_bytes[] = { 0x66, 0x90 };
static char nop5_bytes[] = { 0x0f, 0x1f, 0x44, 0x00, 0x00 };

#define NOP2_ASM \
    ".byte 0x66\n\t" \
    ".byte 0x90\n\t"

#define NOP5_ASM \
    ".byte 0x0f \n\t" \
    ".byte 0x1f \n\t" \
    ".byte 0x44 \n\t" \
    ".byte 0x00 \n\t" \
    ".byte 0x00 \n\t"

struct zcond;
struct ins_point;

static __attribute__((always_inline)) bool arch_zcond_nop(struct zcond *const zcond_p, char ins_type) {
    asm goto(
            "1: " NOP5_ASM
            ZCOND_TABLE_ENTRY
            : : "i" (zcond_p), "i" (ins_type) : : l_true );

    return false;
l_true: return true;
}

static __attribute__((always_inline))  bool arch_zcond_jmp(struct zcond *const zcond_p, char ins_type) {
    asm goto(
            "1: jmp %[l_true] \n\t"
            ZCOND_TABLE_ENTRY
            : : "i" (zcond_p), "i" (ins_type) : : l_true );
    return false;
l_true: return true;
}

void arch_enable_text_write(void);
void arch_disable_text_write(void);

void arch_get_patch_insn(struct ins_point *ins_p, unsigned char insn[], size_t *size);

#endif /*!_MACHINE_ZCOND_H*/
