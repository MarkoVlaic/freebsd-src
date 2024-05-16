#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/zcond.h>

SYSCTL_INT(_kern, OID_AUTO, one, CTLFLAGS_RD, NULL, 1, "Always returns one");


