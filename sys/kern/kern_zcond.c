#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>
#include <sys/zcond.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <sys/smp.h>
#include <sys/cpuset.h>
#include <machine/atomic.h>
#include <machine/cpufunc.h>

MALLOC_DECLARE(M_ZCOND);
MALLOC_DEFINE(M_ZCOND, "zcond", "malloc for the zcond subsystem");

static struct pmap patching_pmap;
//static vm_page_t patch_page_mirror;
//static vm_offset_t mirror_page_addr;

static void 
zcond_init(const void* unused) {
    extern char __zcond_table_start, __zcond_table_end;
    struct ins_point *entry;
    struct zcond *entry_zcond;
    char *entry_addr;
    size_t entry_size;
    extern char kernload, end;
    vm_offset_t kern_start, kern_end;
   
   entry_size = sizeof(struct ins_point);
    
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

    memset(&patching_pmap, 0, sizeof(patching_pmap));
    PMAP_LOCK_INIT(&patching_pmap);
    pmap_pinit(&patching_pmap);
    kern_start = vm_map_max(kernel_map);  
    kern_end = vm_map_min(kernel_map);
    printf("kern start %#08lx | kern end %#08lx | linker end %#08lx\n", kern_start, kern_end, (vm_offset_t)&end); 
    pmap_copy(&patching_pmap, kernel_pmap, kern_start, kern_end - kern_start, kern_start);

    //patch_page_mirror = vm_page_alloc_noobj(VM_ALLOC_WIRED);
    //mirror_page_addr = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(patch_page_mirror));
    //mirror_page_addr = kva_alloc(PAGE_SIZE);
    //pmap_enter(&patching_pmap, mirror_page_addr, patch_page_mirror, VM_PROT_WRITE, PMAP_ENTER_WIRED, 0);
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
    struct ins_point *p;
    unsigned char insn[ZCOND_MAX_INSN_SIZE];
    size_t insn_size;
    int i;
    //vm_page_t patch_page;

    if(cond->enabled == new_state) {
        return;
    }

    SLIST_FOREACH(p, &cond->ins_points, next) {
        zcond_get_patch_insn(p, insn, &insn_size);
        
        printf("patch ins point %#08lx with: ", p->patch_addr);
        for(i=0;i<insn_size;i++) {
            printf("%02hhx ", insn[i]);
        }
        printf("\n");

        //patch_page = PHYS_TO_VM_PAGE(vtophys(p->patch_addr & ~PAGE_MASK));
        
        zcond_before_patch();
        //load_cr3(patching_pmap.pm_cr3);
        //pmap_invlpg(kernel_pmap, p->patch_addr);
        //pmap_zcond_enter(&patching_pmap, mirror_page_addr, patch_page);
        memcpy((void *)(p->mirror_address + (p->patch_addr & PAGE_MASK)), &insn[0], insn_size);
        //invlpg(mirror_page_addr);
        zcond_after_patch();
    }
    cond->enabled = new_state;
}

static void rendezvous_cb(void *arg) {
    struct rendezvous_data *data;
    //uint64_t cr3;

    data = (struct rendezvous_data *)arg;
    if(data->patching_cpu != curcpu) {
       // atomic_add_int(&data->blocked, 1);
       // while(atomic_load_int(&data->patched) == 0) {}
    } else {
       // while(atomic_load_int(&data->blocked) != smp_cpus - 1) {}
        printf("kernel cr3 %#08lx | patching cr3 %#08lx\n", kernel_pmap->pm_cr3, patching_pmap.pm_cr3);
        //cr3 = rcr3();
        //load_cr3(patching_pmap.pm_cr3);  
        zcond_patch(data->cond, data->new_state);
       // load_cr3(cr3);
        //atomic_store_int(&data->patched, 1);
    } 
}

void __zcond_set_enabled(struct zcond *cond, bool new_state) {
    uint64_t cr3;
    struct ins_point *p;
    vm_page_t patch_page;
    struct rendezvous_data arg = {
        .patching_cpu = curcpu,
        .cond = cond,
        .new_state = new_state,
        .blocked = 0,
        .patched = 0
    };
    
    SLIST_FOREACH(p, &cond->ins_points, next) {
        p->mirror_address = kva_alloc(PAGE_SIZE);
        patch_page = PHYS_TO_VM_PAGE(vtophys(p->patch_addr & ~PAGE_MASK));
        pmap_enter(&patching_pmap, p->mirror_address, patch_page, VM_PROT_WRITE, PMAP_ENTER_WIRED, 0);
        printf("patch_point %#08lx mapped to %#08lx\n", p->patch_addr, p->mirror_address);
    }
    
    cr3 = rcr3();
    load_cr3(patching_pmap.pm_cr3);
    smp_rendezvous(NULL, rendezvous_cb, NULL, &arg);    
    load_cr3(cr3);
    
    printf("remove mappings\n");
    SLIST_FOREACH(p, &cond->ins_points, next) {
        //pmap_remove(&patching_pmap, p->mirror_address, p->mirror_address + PAGE_SIZE);
        pmap_qremove(p->mirror_address, 1);
        kva_free(p->mirror_address, PAGE_SIZE);
    }
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
    /*asm (
        ".nops 512\n\t":::    
    );*/

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
