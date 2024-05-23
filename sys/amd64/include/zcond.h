#ifndef _MACHINE_ZCOND_H 
#define _MACHINE_ZCOND_H

//#include <sys/zcond.h>

#define ZCOND_TABLE_ENTRY \
    ".pushsection __zcond_table, \"aw\" \n\t"  \
    ".long 1b - . \n\t" \
    ".long %l[l_true] - . \n\t" \
    ".popsection \n\t"

struct zcond;

static __attribute__((always_inline)) bool arch_zcond_nop(struct zcond *const zcond_p) {
    asm goto(
            "1: .nops 8 \n\t"
            ZCOND_TABLE_ENTRY
            : : [zcond_addr] "i" (zcond_p) : : l_true );

    return false;
l_true: return true;
}

static __attribute__((always_inline)) bool arch_zcond_jmp(struct zcond *const zcond_p) {
    asm goto(
            "1: jmp %[l_true] \n\t"
            ZCOND_TABLE_ENTRY
            : : [zcond_addr] "i" (zcond_p) : : l_true );
    return false;
l_true: return true;
}

#endif /*!_MACHINE_ZCOND_H*/
