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
#include <sys/smp.h>
#include <sys/cpuset.h>
#include <machine/atomic.h>

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

struct rendezvous_data {
    int patching_cpu;
    struct zcond *cond;
    bool new_state;
    int blocked;
    int patched;
};

static void zcond_patch(struct zcond *cond, bool new_state) {
    //critical_enter();
    if(cond->enabled == new_state) {
        return;
    }
     
    struct ins_point *p;
    unsigned char insn[5];
    size_t insn_size;

    SLIST_FOREACH(p, &cond->ins_points, next) {
        arch_get_patch_insn(p, insn, &insn_size);
        
        printf("patch ins point %#08lx with: ", p->patch_addr);
        for(int i=0;i<insn_size;i++) {
            printf("%02hhx ", insn[i]);
        }
        printf("\n");

        arch_enable_text_write();
        memcpy((void *)p->patch_addr, &insn[0], insn_size);
        arch_disable_text_write();
    }
    cond->enabled = new_state;
    //critical_exit();
}

static void rendezvous_cb(void *arg) {
    struct rendezvous_data *data = (struct rendezvous_data *)arg;
    if(data->patching_cpu != curcpu) {
        atomic_add_int(&data->blocked, 1);
        while(atomic_load_int(&data->patched) == 0) {}
    } else {
        while(atomic_load_int(&data->blocked) != smp_cpus - 1) {}
        zcond_patch(data->cond, data->new_state);
        atomic_store_int(&data->patched, 1);
    } 
}

void __zcond_set_enabled(struct zcond *cond, bool new_state) {
    struct rendezvous_data arg = {
        .patching_cpu = curcpu,
        .cond = cond,
        .new_state = new_state,
        .blocked = 0,
        .patched = 0
    };
    smp_rendezvous(NULL, rendezvous_cb, NULL, &arg);    
}

DEFINE_ZCOND_TRUE(cond1);
DEFINE_ZCOND_FALSE(cond2);

static int 
trigger_zcond_test(SYSCTL_HANDLER_ARGS) {
    struct sbuf buf;
    sbuf_new_for_sysctl(&buf, NULL, 256, req);

    sbuf_printf(&buf, "zcond test start\n");
    if(zcond_true(cond1)) {
        sbuf_printf(&buf, "cond 1 true\n");
    } else {
        sbuf_printf(&buf, "cond 1 false\n");
    }
   
    if(zcond_true(cond2)) {
        sbuf_printf(&buf, "cond2 true\n");
    } else {
        sbuf_printf(&buf, "cond2 false\n");
    }

    sbuf_finish(&buf);
    sbuf_delete(&buf);

    return 0;
}

static int trigger_zcond_test2(SYSCTL_HANDLER_ARGS) {
    struct sbuf buf;
    sbuf_new_for_sysctl(&buf, NULL, 256, req);

    sbuf_printf(&buf, "zcond test 2 start\n");
    if(zcond_true(cond1)) {
       sbuf_printf(&buf, "cond 1 true %s\n", __func__);
    }
    
    // simulate long jump with nops
    asm (
        ".nops 512\n\t":::    
    );

    sbuf_finish(&buf);
    sbuf_delete(&buf);
    return 0;
}

static int trigger_zcond_test3(SYSCTL_HANDLER_ARGS) {
    struct sbuf buf;
    sbuf_new_for_sysctl(&buf, NULL, 256, req);
    
    sbuf_printf(&buf, "zcond test 3 start\n");
    if(zcond_false(cond1)) {
        sbuf_printf(&buf, "cond1 false\n");
    } else {
        sbuf_printf(&buf, "cond1 true\n");
    }

    if(zcond_false(cond2)) {
        sbuf_printf(&buf, "cond2 false\n");
    } else {
        sbuf_printf(&buf, "cond2 true\n");
    }

    sbuf_finish(&buf);
    sbuf_delete(&buf);

    return 0;
}

static int zcond_list_inspection_points(SYSCTL_HANDLER_ARGS) {
    struct sbuf buf;
    sbuf_new_for_sysctl(&buf, NULL, 1024, req);

    sbuf_printf(&buf, "inspection points for cond1:\n");
    struct ins_point *p;
    SLIST_FOREACH(p, &cond1.cond.ins_points, next) {
        sbuf_printf(&buf, "patch_addr = %#08lx | jump_addr = %#08lx | zcond_ptr = %p\n", p->patch_addr, p->lbl_true_addr, p->zcond);
    }
    
    sbuf_printf(&buf, "inspection points for cond2:\n");
    SLIST_FOREACH(p, &cond2.cond.ins_points, next) {
        sbuf_printf(&buf, "patch_addr = %#08lx | jump_addr = %#08lx | zcond_ptr = %p\n", p->patch_addr, p->lbl_true_addr, p->zcond);
    }

    sbuf_finish(&buf);
    sbuf_delete(&buf);

    return 0;
}

static int zcond1_disable(SYSCTL_HANDLER_ARGS) {
    struct sbuf buf;
    sbuf_new_for_sysctl(&buf, NULL, 256, req);

    zcond_disable(cond1);
    sbuf_printf(&buf, "cond1 disabled\n");

    return 0;
}


static int zcond1_enable(SYSCTL_HANDLER_ARGS) { 
    struct sbuf buf;
    sbuf_new_for_sysctl(&buf, NULL, 256, req);
    
    zcond_enable(cond1);
    sbuf_printf(&buf, "cond1 enabled\n");
    
    sbuf_finish(&buf);
    sbuf_delete(&buf);
    return 0;
}

static int zcond2_disable(SYSCTL_HANDLER_ARGS) {
    struct sbuf buf;
    sbuf_new_for_sysctl(&buf, NULL, 256, req);

    zcond_disable(cond2);
    sbuf_printf(&buf, "cond2 disabled\n");

    return 0;
}


static int zcond2_enable(SYSCTL_HANDLER_ARGS) { 
    struct sbuf buf;
    sbuf_new_for_sysctl(&buf, NULL, 256, req);
    
    zcond_enable(cond2);
    sbuf_printf(&buf, "cond2 enabled\n");
    
    sbuf_finish(&buf);
    sbuf_delete(&buf);

    return 0;
}
SYSCTL_PROC(_kern, OID_AUTO, zcond, CTLFLAG_RD | CTLTYPE_STRING, NULL, 0, trigger_zcond_test, "I", "trigger zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond2, CTLFLAG_RD | CTLTYPE_STRING, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test2, "I", "trigger second zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond3, CTLFLAG_RD | CTLTYPE_STRING, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test3, "I", "trigger third zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond_ins_p, CTLFLAG_RD | CTLTYPE_STRING, SYSCTL_NULL_INT_PTR, 0, zcond_list_inspection_points, "I", "list cond1 inspection points");
SYSCTL_PROC(_kern, OID_AUTO, zcond1_enable, CTLFLAG_RD | CTLTYPE_STRING, SYSCTL_NULL_INT_PTR, 0, zcond1_enable, "I", "enable zcond1");
SYSCTL_PROC(_kern, OID_AUTO, zcond1_disable, CTLFLAG_RD | CTLTYPE_STRING, SYSCTL_NULL_INT_PTR, 0, zcond1_disable, "I", "disable zcond1");
SYSCTL_PROC(_kern, OID_AUTO, zcond2_enable, CTLFLAG_RD | CTLTYPE_STRING, SYSCTL_NULL_INT_PTR, 0, zcond2_enable, "I", "enable zcond2");
SYSCTL_PROC(_kern, OID_AUTO, zcond2_disable, CTLFLAG_RD | CTLTYPE_STRING, SYSCTL_NULL_INT_PTR, 0, zcond2_disable, "I", "disable zcond2");
