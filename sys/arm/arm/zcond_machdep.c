#include <machine/zcond.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>

#define INSN_SIZE 4

static const uint32_t nop_insn = 0xe320f000u;

static bool
patch_addr_valid(uintptr_t patch_addr, uintptr_t lbl_true_addr)
{
	void *addr;
	int32_t offset;

	if (!arm64_get_writable_addr((void *)patch_addr, &addr))
		return (false);

	if (patch_addr == lbl_true_addr ||
	    (patch_addr & (INSN_SIZE - 1)) != 0 ||
	    (lbl_true_addr & (INSN_SIZE - 1)) != 0 ||
        patch_addr + 2 * INSN_SIZE < patch_addr)
		return (false);
	offset = lbl_true_addr - (patch_addr + 2 * INSN_SIZE);
	if (offset < -(1 << 24) || offset > (1 << 24))
		return (false);
	return (true);
}

void
zcond_patchpoint_patch(uintptr_t patch_addr, uintptr_t lbl_true_addr)
{
    uint32_t instr;

	KASSERT(patch_addr_valid(patch_addr, lbl_true_addr),
	    ("%s: invalid tracepoint %#x -> %#x",
	    __func__, patch_addr, lbl_true_addr));

    if(*((uint32_t*)patch_addr) == nop_insn) {
        // Replace nop with jump
        instr =
            (((lbl_true_addr - (patch_addr + 2 * INSN_SIZE)) >> 2) & ((1 << 24) - 1)) |
            0xea000000;
    } else {
        // Replace jump with nop
        instr = nop_insn;
    }

	memcpy(addr, &instr, sizeof(instr));
	icache_sync(patch_addr, INSN_SIZE);
}
