#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/zcond.h>

DEFINE_ZCOND_TRUE(cond1);
DEFINE_ZCOND_FALSE(cond2);


static int 
trigger_zcond_test(SYSCTL_HANDLER_ARGS) {
    printf("zcond test start\n");
    if(zcond_true(cond1)) {
        printf("cond 1 true\n");
    }

    if(zcond_false(cond1)) {
        printf("cond 1 false\n");
    }
    
    if(zcond_true(cond2)) {
        printf("cond 2 true\n");
    }

    if(zcond_false(cond2)) {
        printf("cond 2 false\n");
    }
    return 0;
}

static int trigger_zcond_test2(SYSCTL_HANDLER_ARGS) {
    printf("zcond test 2 start\n");
    if(zcond_true(cond1)) {
        printf("cond 1 true\n");
    }

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

SYSCTL_PROC(_kern, OID_AUTO, zcond, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test, "I", "trigger zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond2, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test2, "I", "trigger second zcond test");
SYSCTL_PROC(_kern, OID_AUTO, zcond3, CTLFLAG_RW | CTLTYPE_INT, SYSCTL_NULL_INT_PTR, 0, trigger_zcond_test3, "I", "trigger third zcond test");
