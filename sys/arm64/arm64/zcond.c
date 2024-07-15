#include <machine/zcond.h>
#include <sys/zcond.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <amd64/vmm/amd/svm.h>
#include <machine/md_var.h>

static bool wp;
void arch_enable_text_write(void) {
    wp = disable_wp();
}

void arch_disable_text_write(void) {
    restore_wp(wp);
}


static void arch_insn_nop(unsigned char insn[]) {
    int i;
    for(i=0;i<4;i++) {
        insn[i] = nop_bytes[i];
    }
}

static void arch_insn_jmp(unsigned char insn[], vm_offset_t offset) {
    vm_offset_t imm26 = offset >> 2;
    insn[0] = 0x5 | ( (imm26 >> 24) & 0x3 );
    int i;
    for(i=0;i<3;i++) {
        insn[i+1] = (imm26 >> (i*8)) & 0xFF;
    }
}

void arch_get_patch_insn(struct ins_point *p, unsigned char insn[], size_t *size) {
    unsigned char *patch_addr = (unsigned char*) p->patch_addr;

    if(*patch_addr == nop_bytes[0]) {
        vm_offset_t offset = p->lbl_true_addr - p->patch_addr;
        arch_insn_jmp(insn, offset);
    } else if(*patch_addr & ~(0x3) == 0x5){
        arch_insn_nop(insn);
    } else {
        panic("unexpected opcode: %02hhx", *patch_addr); 
    }
}
