#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>
#include <sys/zcond.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <machine/md_var.h>

MALLOC_DECLARE(M_ZCOND);
MALLOC_DEFINE(M_ZCOND, "zcond", "malloc for the zcond subsystem");

static void 
zcond_init(void* unused) {
    extern char __zcond_table_start, __zcond_table_end;
    struct ins_point *entry;
    struct zcond *entry_zcond;
    char *entry_addr;
    size_t entry_size = sizeof(struct ins_point);
    
    for(entry_addr = &__zcond_table_start; entry_addr < &__zcond_table_end; entry_addr += entry_size) {
        entry = (struct ins_point *) entry_addr; 
        entry_zcond = entry->zcond;

        if(entry_zcond->ins_points.slh_first == NULL) {
            SLIST_INIT(&entry_zcond->ins_points);
        }

        SLIST_INSERT_HEAD(&entry_zcond->ins_points, entry, next);    
       
        /*if(zcond->swap_page == NULL) { 
            vm_page_t page = vm_page_alloc_noobj(VM_ALLOC_WIRED);
            entry_zcond->swap_page_vaddr = kva_alloc(PAGE_SIZE);
            
            pmap_enter(kernel_pmap, entry_zcond->swap_page_vaddr, page, VM_PROT_READ | VM_PROT_WRITE | PMAP_ENTER_WIRED, 0);

            bcopy(entry->patch_addr & ~PAGE_MASK, entry_zcond->swap_page_vaddr, PAGE_SIZE);
        }*/

    }
}
SYSINIT(zcond, SI_SUB_LAST, SI_ORDER_ANY, zcond_init, NULL); // do we declare a new SI_SUB? is the order important?


void __zcond_enable(struct zcond* cond) {
    if(cond->enabled) {
        return;
    }
    
    struct ins_point *p;
    unsigned char* patch_addr;
    unsigned char insn[5];
    size_t insn_size;
    SLIST_FOREACH(p, &cond->ins_points, next) {
        bool wp = disable_wp();
        patch_addr = (char*) p->patch_addr;

        if(p->ins_type == INS_TYPE_TRUE) {
            // replace nop with jmp
            vm_offset_t offset;
            if(*patch_addr == 0x66) {
                // two byte nop
               insn_size = 2;
            } else if(*patch_addr == 0x0f) {
                insn_size = 5;
            } else {
                panic("unexpected opcode: %02hhx", *patch_addr); 
            }
            
            offset = p->lbl_true_addr - p->patch_addr - insn_size; 
            arch_insn_jmp(insn, insn_size, offset);
            printf("offset = %#08lx\n", offset);
        } else {
            //  replace jmp with nop
            if(*patch_addr == 0xeb) {
                // two byte jump
                insn_size = 2;
            } else if(*patch_addr == 0xe9) {
                // five byte jump
                insn_size = 5;
            } else {
                panic("unexpected opcode: %02hhx", *patch_addr); 
            }
            arch_insn_nop(insn, insn_size);
        }
        
        printf("patch ins point %#08lx with: ", p->patch_addr);
        for(int i=0;i<insn_size;i++) {
            printf("%02hhx ", insn[i]);
        }
        printf("\n");
        memcpy((void *)patch_addr, &insn[0], insn_size);
        restore_wp(wp);
    }
    cond->enabled = true;
}

void __zcond_disable(struct zcond* cond) {
    if(!cond->enabled) {
        return;
    }
    
    struct ins_point *p;
    unsigned char* patch_addr;
    unsigned char insn[5];
    size_t insn_size;
    SLIST_FOREACH(p, &cond->ins_points, next) {
        bool wp = disable_wp();
        patch_addr = (char*) p->patch_addr;

        if(p->ins_type == INS_TYPE_FALSE) {
            // replace nop with jmp
            vm_offset_t offset;
            if(*patch_addr == 0x66) {
                // two byte nop
               insn_size = 2;
            } else if(*patch_addr == 0x0f) {
                insn_size = 5;
            } else {
                panic("unexpected opcode: %02hhx", *patch_addr); 
            }
            
            offset = p->lbl_true_addr - p->patch_addr - insn_size; 
            arch_insn_jmp(insn, insn_size, offset);
            printf("offset = %#08lx\n", offset);
        } else {
            //  replace jmp with nop
            if(*patch_addr == 0xeb) {
                // two byte jump
                insn_size = 2;
            } else if(*patch_addr == 0xe9) {
                // five byte jump
                insn_size = 5;
            } else {
                panic("unexpected opcode: %02hhx", *patch_addr); 
            }
            arch_insn_nop(insn, insn_size);
        }
        
        printf("patch ins point %#08lx with: ", p->patch_addr);
        for(int i=0;i<insn_size;i++) {
            printf("%02hhx ", insn[i]);
        }
        printf("\n");
        memcpy((void *)patch_addr, &insn[0], insn_size);
        restore_wp(wp);
    }
    cond->enabled = false;
}

DEFINE_ZCOND_TRUE(cond1);
// ZCOND_INIT(&cond1); where should i call this?
DEFINE_ZCOND_FALSE(cond2);
// ZCOND_INIT(&cond2);


static int 
trigger_zcond_test(SYSCTL_HANDLER_ARGS) {
    struct sbuf buf;
    sbuf_new_for_sysctl(&buf, NULL, 256, req);

    sbuf_printf(&buf, "zcond test start\n");
    if(zcond_true(cond1)) {
        sbuf_printf(&buf, "cond 1 true\n");
        asm (
            ".nops 512\n\t":::    
        );
    }
    
    sbuf_finish(&sbuf);
    sbuf_delete(&sbuf);
    
    /*if(zcond_false(cond1)) {
        printf("cond 1 false\n");
    }*/
    
   /* if(zcond_true(cond2)) {
        printf("cond 2 true\n");
    }

    if(zcond_false(cond2)) {
        printf("cond 2 false\n");
    }*/
    return 0;
}

static int trigger_zcond_test2(SYSCTL_HANDLER_ARGS) {
    printf("zcond test 2 start\n");
    if(zcond_true(cond1)) {
        printf("cond 1 true %s\n", __func__);
    }
    asm (
        ".nops 512\n\t":::    
    );
    return 0;
}

static int trigger_zcond_test3(SYSCTL_HANDLER_ARGS) {
    printf("zcond test 3 start\n");
    if(zcond_false(cond1)) {
        printf("cond 1 false\n");
    } else {
        printf("else branch\n");
    }

    return 0;
}

static int zcond_list_inspection_points(SYSCTL_HANDLER_ARGS) {
    printf("inspection points for cond1:\n");
    struct ins_point *p;
    SLIST_FOREACH(p, &cond1.cond.ins_points, next) {
        printf("patch_addr = %#08lx | jump_addr = %#08lx | zcond_ptr = %p | ins_type = %d\n", p->patch_addr, p->lbl_true_addr, p->zcond, p->ins_type);
    }
    
    return 0;
}

static int zcond1_disable(SYSCTL_HANDLER_ARGS) {
    zcond_disable(cond1);
    printf("disabled\n");

    return 0;
}


static int zcond1_enable(SYSCTL_HANDLER_ARGS) {
    zcond_enable(cond1);
    printf("enabled\n");

    return 0;
}

SYSCTL_PROC(_kern, OID_AUTO, zcond, CTLFLAG_RW | CTLTYPE_STRING, NULL, 0, trigger_zcond_test, "I", "trigger zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond2, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test2, "I", "trigger second zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond3, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test3, "I", "trigger third zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond_ins_p, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, zcond_list_inspection_points, "I", "list cond1 inspection points");
SYSCTL_PROC(_kern, OID_AUTO, zcond1_enable, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, zcond1_enable, "I", "enable zcond1");
SYSCTL_PROC(_kern, OID_AUTO, zcond1_disable, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, zcond1_disable, "I", "disable zcond1");
