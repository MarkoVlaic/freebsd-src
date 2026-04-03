#include <machine/zcond.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>

#define INSN_SIZE 4

static const uint32_t nop_insn = 0x60000000;

static bool
patch_addr_valid(uintptr_t patch_addr, uintptr_t lbl_true_addr)
{

	int64_t offset;

	if (patchpoint == target ||
	    (patchpoint & 3) != 0 || (target & 3) != 0)
		return (false);
	offset = target - patchpoint;
	if (offset < -(1 << 26) || offset > (1 << 26))
		return (false);
	return (true);
}

void
zcond_patchpoint_patch(uintptr_t patch_addr, uintptr_t lbl_true_addr)
{
    uint32_t instr;

	KASSERT(patch_addr_valid(patch_addr, lbl_true_addr),
	    ("%s: invalid tracepoint %#lx -> %#lx",
	    __func__, (uintmax_t)patch_addr, (uintmax_t)lbl_true_addr));

    if(*((uint32_t*)patch_addr) == nop_insn) {
        // Replace nop with jump
        instr = ((patch_addr - lbl_true_addr) & 0x7fffffful) | 0x48000000;
    } else {
        // Replace jump with nop
        instr = nop_insn;
    }

	memcpy((void *)patch_addr, &instr, sizeof(instr));
	__syncicache((void *)patchpoint, sizeof(instr));
}
