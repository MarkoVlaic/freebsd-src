#ifndef _MACHINE_ZCOND_H 
#define _MACHINE_ZCOND_H


static __attribute__((always_inline)) bool arch_zcond_nop(void) {
    asm goto(
            ".nops 8 \n\t"
            : : : : l_true );

    return false;
l_true: return true;
}

static __attribute__((always_inline)) bool arch_zcond_jmp(void) {
    asm goto(
            "jmp %[l_true] \n\t"
            : : : : l_true );
    return false;
l_true: return true;
}

#endif /*!_MACHINE_ZCOND_H*/
