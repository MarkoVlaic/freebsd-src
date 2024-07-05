#ifndef _MACHINE_ZCOND_H 
#define _MACHINE_ZCOND_H

//#include <sys/zcond.h>

#define ZCOND_TABLE_ENTRY \
    ".pushsection __zcond_table, \"aw\" \n\t"  \
    ".quad 1b \n\t" \
    ".quad %l[l_true] \n\t" \
    ".quad %c0 \n\t" \
    ".byte %c1 \n\t" \
    ".quad 0 \n\t" \
    ".quad 0 \n\t" \
    ".popsection \n\t"

// from Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 2B 4-165 
#define NOP2_BYTES { 0x66, 0x90 }
#define NOP5_BYTES { 0x0f, 0x1f, 0x44, 0x00, 0x00 }

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



#endif /*!_MACHINE_ZCOND_H*/
