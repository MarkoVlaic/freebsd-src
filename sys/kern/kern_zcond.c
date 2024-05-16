#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/zcond.h>

SYSCTL_INT(_kern, OID_AUTO, one, CTLFLAG_RD, SYSCTL_NULL_INT_PTR, 1, "Always returns one");


