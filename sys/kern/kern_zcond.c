#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/zcond.h>
#include <sys/malloc.h>

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
    }
}
SYSINIT(zcond, SI_SUB_LAST, SI_ORDER_ANY, zcond_init, NULL); // do we declare a new SI_SUB? is the order important?

DEFINE_ZCOND_TRUE(cond1);
// ZCOND_INIT(&cond1); where should i call this?
DEFINE_ZCOND_FALSE(cond2);
// ZCOND_INIT(&cond2);


static int 
trigger_zcond_test(SYSCTL_HANDLER_ARGS) __attribute__((optnone)) {
    printf("zcond test start\n");
    if(zcond_true(cond1)) {
            printf("cond 1 true\n");
        }

    if(zcond_false(cond1)) {
        printf("cond 1 false\n");
    }
    
   /* if(zcond_true(cond2)) {
        printf("cond 2 true\n");
    }

    if(zcond_false(cond2)) {
        printf("cond 2 false\n");
    }*/
    return 0;
}

static int trigger_zcond_test2(SYSCTL_HANDLER_ARGS) __attribute__((optnone)) {
    printf("zcond test 2 start\n");
    if(zcond_true(cond1)) {
        printf("cond 1 true %s\n", __func__);
    }

    return 0;
}

static int trigger_zcond_test3(SYSCTL_HANDLER_ARGS) __attribute__((optnone)) {
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
        printf("patch_addr = %#08lx | jump_addr = %#08lx | zcond_ptr = %p\n", p->patch_addr, p->lbl_true_addr, p->zcond);
    }
    
    return 0;
}

SYSCTL_PROC(_kern, OID_AUTO, zcond, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test, "I", "trigger zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond2, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test2, "I", "trigger second zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond3, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test3, "I", "trigger third zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond_ins_p, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, zcond_list_inspection_points, "I", "list cond1 inspection points");
