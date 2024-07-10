#include <machine/zcond.h>
#include <machine/md_var.h>
#include <vm/vm.h>

static bool wp;
void arch_enable_text_write(void) {
    wp = disable_wp();
}

void arch_disable_text_write(void) {
    restore_wp(wp);
}

static void arch_insn_nop(unsigned char insn[], size_t size) {
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

static void arch_insn_jmp(unsigned char insn[], size_t size, vm_offset_t offset) {
    if(size == 2) {
        insn[0] = 0xeb;
        insn[1] = offset;
    } else {
        insn[0] = 0xe9;
        int i;
        for(i=0;i<4;i++) {
            insn[i+1] = (offset >> (i*8)) & 0xFF;
        }
    }
}

void arch_get_patch_insn(struct zcond *cond, struct ins_point *ins_p, bool new_state, unsigned char insn[], size_t *size) {
    unsigned char *patch_addr = (unsigned char*) ins_p->patch_addr;

    if( (ins_p->ins_type == INS_TYPE_TRUE && new_state) || (ins_p->ins_type == INS_TYPE_FALSE && !new_state)) {
        // replace nop with jmp
        vm_offset_t offset;
        if(*patch_addr == 0x66) {
            // two byte nop
           *size = 2;
        } else if(*patch_addr == 0x0f) {
            *size = 5;
        } else {
            panic("unexpected opcode: %02hhx", *patch_addr); 
        }
        
        offset = p->lbl_true_addr - p->patch_addr - *size; 
        arch_insn_jmp(insn, *size, offset);
        printf("offset = %#08lx\n", offset);
    } else {
        //  replace jmp with nop
        if(*patch_addr == 0xeb) {
            // two byte jump
            *size = 2;
        } else if(*patch_addr == 0xe9) {
            // five byte jump
            *size = 5;
        } else {
            panic("unexpected opcode: %02hhx", *patch_addr); 
        }
        arch_insn_nop(insn, *size);
    }
}



