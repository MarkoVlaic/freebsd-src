#include <machine/zcond.h>
#include <sys/vm.h>

void arch_insn_nop(unsigned char insn[], size_t size) {
    int i;
    if(size == 2) {
        for(i=0;i<2;i++) {
            insn[i] = nop2_bytes[i];
        }
    } else {
        for(i=0;i<5;i++) {
            insn[i] = nop5_bytes[i];
        }
    }
}

void arch_insn_jmp(unsigned char insn[], size_t size, vm_offset_t offset) {
    if(size == 2) {
        insn[0] = 0xeb;
        insn[1] = offset;
    } else {
        insn[0] = 0xe9;
        int i;
        for(i=0;i<4;i++) {
            insn[i+1] = (offset >> (i*2)) & 0xFF;
        }
    }
}
