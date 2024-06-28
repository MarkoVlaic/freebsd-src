#ifndef _MACHINE_ZCOND_H 
#define _MACHINE_ZCOND_H

//#include <sys/zcond.h>

#define ZCOND_TABLE_ENTRY \
    ".pushsection __zcond_table, \"aw\" \n\t"  \
    ".quad 1b \n\t" \
    ".quad %l[l_true] \n\t" \
    ".quad %c0 \n\t" \
    ".quad 0 \n\t" \
    ".quad 0 \n\t" \
    ".popsection \n\t"

struct zcond;

static __attribute__((always_inline)) bool arch_zcond_nop(struct zcond *const zcond_p) {
    asm goto(
            "1: .nops 5 \n\t"
            ZCOND_TABLE_ENTRY
            : : "i" (zcond_p) : : l_true );

    return false;
l_true: return true;
}

static __attribute__((always_inline))  bool arch_zcond_jmp(struct zcond *const zcond_p) {
    asm goto(
            "1: jmp %[l_true] \n\t"
            ZCOND_TABLE_ENTRY
            : : "i" (zcond_p) : : l_true );
    return false;
l_true: return true;
}

// from Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 2B 4-165 
#define NOP_BYTES {0x0F, 0x1F, 0x44, 0x00, 0x00}
#define NOP_SIZE 5

#endif /*!_MACHINE_ZCOND_H*/
